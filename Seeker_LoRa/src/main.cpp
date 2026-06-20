#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <PubSubClient.h>
#include <RadioLib.h>
#include <SPI.h>
#include <TinyGPSPlus.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <Wire.h>
#include <XPowersLib.h>

const unsigned long STATUS_INTERVAL_MS = 2000;
const unsigned long DISPLAY_INTERVAL_MS = 500;
const unsigned long GPS_FIX_MAX_AGE_MS = 5000;
const unsigned long TAG_LOST_MS = 6000;
const unsigned long HIDER_ALERT_MS = 20000;
const unsigned long ACTIVE_ALERT_PACKET_MS = 5000;
const unsigned long BLE_SCAN_INTERVAL_MS = 3000;
const unsigned long BLE_SEEN_MAX_AGE_MS = 8000;
const unsigned long BUTTON_DEBOUNCE_MS = 50;
const unsigned long WIFI_RETRY_MS = 10000;
const unsigned long MQTT_RETRY_MS = 5000;
const unsigned long MQTT_SUCCESS_LOG_MS = 10000;
const unsigned long HEARTBEAT_FLASH_MS = 500;
const int BLE_SCAN_SECONDS = 1;
const int BLE_HANDOFF_SAMPLES = 3;
const int BLE_ENTER_RSSI = -67;
const int BLE_EXIT_RSSI = -78;

const int GPS_RX = 34;
const int GPS_TX = 12;
const int I2C_SDA = 21;
const int I2C_SCL = 22;
const int OLED_RST = 16;
const int USER_BUTTON = 38;

const int LORA_SCK = 5;
const int LORA_MISO = 19;
const int LORA_MOSI = 27;
const int LORA_CS = 18;
const int LORA_RST = 23;
const int LORA_DIO0 = 26;
const int LORA_DIO1 = 33;

const float LORA_FREQ_MHZ = 915.0;
const char *BLE_SEEKER_NAME = "MP-SEEKER01";
const char *BLE_HIDER_NAME = "MP-HIDER01";
const char *WIFI_SSID = "MarcoPolo";
const char *WIFI_PASSWORD = "MarcoPolo123";
const char *MQTT_BROKER = "10.42.0.1";
const int MQTT_PORT = 1883;
const char *MQTT_TOPIC = "marcopolo/seeker/telemetry";
const char *MQTT_CLIENT_ID = "marcopolo-seeker01";

TinyGPSPlus gps;
HardwareSerial gpsSerial(1);
XPowersPMU pmu;
SX1276 radio = new Module(LORA_CS, LORA_DIO0, LORA_RST, LORA_DIO1);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, OLED_RST);
BLEScan *bleScan = nullptr;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

bool radioReady = false;
bool pmuReady = false;
bool displayReady = false;
bool bleReady = false;
volatile bool packetReceived = false;

bool hiderFixValid = false;
double hiderLat = 0.0;
double hiderLon = 0.0;
unsigned long hiderSats = 0;
double hiderHdop = 0.0;
unsigned long hiderSeq = 0;
unsigned long hiderUptimeMs = 0;
unsigned long lastHiderPacketMs = 0;
unsigned long lastHiderAlertMs = 0;
float hiderBatteryV = 0.0;
float hiderRssi = 0.0;
float hiderSnr = 0.0;
int hiderBatteryPct = 0;
bool hiderMotion = false;
bool hiderMotionValid = false;
bool hiderBatteryVValid = false;
bool hiderBatteryPctValid = false;
bool hiderUptimeValid = false;
bool hiderRssiValid = false;
bool hiderSnrValid = false;
int hiderBleRssi = 0;
bool hiderBleRssiValid = false;
bool bleHandoffActive = false;
int strongBleSamples = 0;
int weakBleSamples = 0;
unsigned long lastBleSeenMs = 0;
double currentDistanceFeet = -1.0;
double currentBearingDeg = 0.0;
bool screenTwo = false;
bool lastButtonReading = HIGH;
bool buttonState = HIGH;
bool wifiWasConnected = false;

unsigned long lastStatusMs = 0;
unsigned long lastDisplayMs = 0;
unsigned long lastBleScanMs = 0;
unsigned long lastButtonChangeMs = 0;
unsigned long lastWiFiAttemptMs = 0;
unsigned long lastMqttAttemptMs = 0;
unsigned long lastMqttSuccessLogMs = 0;
unsigned long lastPacketPulseMs = 0;

void publishTelemetry();

void onPacketReceived() {
  packetReceived = true;
}

bool bleSeenRecently() {
  return lastBleSeenMs > 0 && millis() - lastBleSeenMs < BLE_SEEN_MAX_AGE_MS;
}

class HiderBleCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice device) override {
    if (!device.haveName() || device.getName() != BLE_HIDER_NAME) {
      return;
    }

    hiderBleRssi = device.getRSSI();
    hiderBleRssiValid = true;
    lastBleSeenMs = millis();

    if (!bleHandoffActive) {
      strongBleSamples = hiderBleRssi >= BLE_ENTER_RSSI ? strongBleSamples + 1 : 0;
      if (strongBleSamples >= BLE_HANDOFF_SAMPLES) {
        bleHandoffActive = true;
        strongBleSamples = 0;
        weakBleSamples = 0;
      }
      return;
    }

    weakBleSamples = hiderBleRssi <= BLE_EXIT_RSSI ? weakBleSamples + 1 : 0;
    if (weakBleSamples >= BLE_HANDOFF_SAMPLES) {
      bleHandoffActive = false;
      strongBleSamples = 0;
      weakBleSamples = 0;
    }
  }
};

HiderBleCallbacks hiderBleCallbacks;

void setupPower() {
  pmuReady = pmu.begin(Wire, AXP192_SLAVE_ADDRESS, I2C_SDA, I2C_SCL);
  if (!pmuReady) {
    return;
  }

  pmu.setLDO2Voltage(3300);
  pmu.enableLDO2();
  pmu.setLDO3Voltage(3300);
  pmu.enableLDO3();
  pmu.setDC1Voltage(3300);
  pmu.enableDC1();
  pmu.enableDC3();
  pmu.enableBattDetection();
  pmu.enableBattVoltageMeasure();
  pmu.setPowerKeyPressOffTime(XPOWERS_AXP192_POWEROFF_4S);
  pmu.setPowerKeyPressOnTime(XPOWERS_POWERON_128MS);
}

bool i2cDevicePresent(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void setupDisplay() {
  delay(200);

  bool found3c = i2cDevicePresent(0x3C);
  bool found3d = i2cDevicePresent(0x3D);
  display.setI2CAddress((found3d ? 0x3D : 0x3C) * 2);
  display.begin();
  display.setPowerSave(0);
  display.setContrast(255);
  displayReady = found3c || found3d;
}

void readGps() {
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }
}

void setupMqtt() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setSocketTimeout(1);
  mqttClient.setKeepAlive(15);
  mqttClient.setBufferSize(1024);
  lastWiFiAttemptMs = millis();
  Serial.println("Wi-Fi connecting");
}

void maintainWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiWasConnected) {
      wifiWasConnected = true;
      Serial.print("Wi-Fi connected: ");
      Serial.println(WiFi.localIP());
    }
    return;
  }

  wifiWasConnected = false;

  if (millis() - lastWiFiAttemptMs < WIFI_RETRY_MS) {
    return;
  }

  lastWiFiAttemptMs = millis();
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Wi-Fi reconnecting");
}

void maintainMqtt() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (mqttClient.connected()) {
    mqttClient.loop();
    return;
  }

  if (millis() - lastMqttAttemptMs < MQTT_RETRY_MS) {
    return;
  }

  lastMqttAttemptMs = millis();
  if (mqttClient.connect(MQTT_CLIENT_ID)) {
    Serial.println("MQTT connected");
  } else {
    Serial.print("MQTT connect failed: ");
    Serial.println(mqttClient.state());
  }
}

bool seekerGpsHasFix() {
  return gps.location.isValid() && gps.location.age() < GPS_FIX_MAX_AGE_MS;
}

unsigned long seekerSatCount() {
  return gps.satellites.isValid() ? gps.satellites.value() : 0;
}

bool seekerHdopValid() {
  return gps.hdop.isValid();
}

bool hiderSeenRecently() {
  return lastHiderPacketMs > 0 && millis() - lastHiderPacketMs < TAG_LOST_MS;
}

bool hiderMovingRecently() {
  return lastHiderAlertMs > 0 && millis() - lastHiderAlertMs < HIDER_ALERT_MS;
}

bool hiderAlertPacketsActive() {
  return lastHiderAlertMs > 0 && millis() - lastHiderAlertMs < ACTIVE_ALERT_PACKET_MS;
}

int batteryPercentFromVoltage(uint16_t millivolts) {
  if (millivolts <= 3300) {
    return 0;
  }
  if (millivolts >= 4200) {
    return 100;
  }

  return map(millivolts, 3300, 4200, 0, 100);
}

int batteryPercent() {
  if (!pmuReady || !pmu.isBatteryConnect()) {
    return -1;
  }

  int percent = pmu.getBatteryPercent();
  if (percent >= 0 && percent <= 100) {
    return percent;
  }

  return batteryPercentFromVoltage(pmu.getBattVoltage());
}

bool useBleRssi() {
  if (!bleSeenRecently()) {
    bleHandoffActive = false;
    strongBleSamples = 0;
    weakBleSamples = 0;
  }

  return bleHandoffActive && hiderBleRssiValid;
}

bool selectedRssi(float &rssi) {
  if (useBleRssi()) {
    rssi = hiderBleRssi;
    return true;
  }

  if (hiderSeenRecently() && hiderRssiValid) {
    rssi = hiderRssi;
    return true;
  }

  return false;
}

const char *heatLabel() {
  float rssi = 0.0;
  if (!selectedRssi(rssi)) {
    return "UNK";
  }

  if (useBleRssi()) {
    if (rssi > -45.0) {
      return "ON FIRE!";
    }
    if (rssi >= -55.0) {
      return "VERY HOT";
    }
    if (rssi >= -65.0) {
      return "HOT";
    }
    return "WARM";
  }

  if (rssi > -30.0) {
    return "ON FIRE!";
  }
  if (rssi >= -40.0) {
    return "VERY HOT";
  }
  if (rssi >= -50.0) {
    return "HOT";
  }
  if (rssi >= -60.0) {
    return "WARM";
  }
  if (rssi >= -70.0) {
    return "LUKE WARM";
  }
  if (rssi >= -80.0) {
    return "COOL";
  }
  if (rssi >= -90.0) {
    return "COLD";
  }
  if (rssi >= -100.0) {
    return "VERY COLD";
  }
  return "FROZEN";
}

const char *cardinalDirection() {
  int sector = (int)((currentBearingDeg + 22.5) / 45.0) % 8;
  const char *directions[] = {
      "NORTH", "NORTHEAST", "EAST", "SOUTHEAST",
      "SOUTH", "SOUTHWEST", "WEST", "NORTHWEST"};
  return directions[sector];
}

const char *distanceBlock() {
  if (currentDistanceFeet < 0.0 && useBleRssi()) {
    if (hiderBleRssi >= -55) {
      return "<10 FT";
    }
    if (hiderBleRssi >= -65) {
      return "<25 FT";
    }
    return "<50 FT";
  }

  if (currentDistanceFeet < 0.0) {
    float rssi = 0.0;
    if (!selectedRssi(rssi)) {
      return "-- FT";
    }
    if (rssi > -35.0) {
      return "<10 FT";
    }
    if (rssi >= -45.0) {
      return "<20 FT";
    }
    if (rssi >= -55.0) {
      return "<50 FT";
    }
    if (rssi >= -65.0) {
      return "<100 FT";
    }
    if (rssi >= -75.0) {
      return "<250 FT";
    }
    if (rssi >= -85.0) {
      return "<500 FT";
    }
    return ">500 FT";
  }
  if (currentDistanceFeet < 10.0) {
    return "<10 FT";
  }
  if (currentDistanceFeet < 20.0) {
    return "<20 FT";
  }
  if (currentDistanceFeet < 50.0) {
    return "<50 FT";
  }
  if (currentDistanceFeet < 100.0) {
    return "<100 FT";
  }
  if (currentDistanceFeet < 250.0) {
    return "<250 FT";
  }
  if (currentDistanceFeet < 500.0) {
    return "<500 FT";
  }
  return ">500 FT";
}

const char *activeLockLabel() {
  if (seekerGpsHasFix() && hiderSeenRecently() && hiderFixValid) {
    return "GPS Lock";
  }
  if (useBleRssi()) {
    return "BLE Lock";
  }
  if (hiderSeenRecently()) {
    return "LoRa Lock";
  }
  return "No Lock";
}

int proximityPercent() {
  if (currentDistanceFeet >= 0.0) {
    if (currentDistanceFeet <= 10.0) {
      return 100;
    }
    if (currentDistanceFeet >= 500.0) {
      return 5;
    }
    return map((long)currentDistanceFeet, 500, 10, 5, 100);
  }

  float rssi = 0.0;
  if (!selectedRssi(rssi)) {
    return 0;
  }
  if (useBleRssi()) {
    return constrain(map((long)rssi, -85, -35, 55, 100), 55, 100);
  }
  return constrain(map((long)rssi, -105, -35, 0, 100), 0, 100);
}

void updateHuntMetrics() {
  if (!seekerGpsHasFix() || !hiderSeenRecently() || !hiderFixValid) {
    currentDistanceFeet = -1.0;
    return;
  }

  double meters = TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), hiderLat, hiderLon);
  currentDistanceFeet = meters * 3.28084;
  currentBearingDeg = TinyGPSPlus::courseTo(gps.location.lat(), gps.location.lng(), hiderLat, hiderLon);
}

void drawCenteredLine(const char *text, int y) {
  int x = (display.getDisplayWidth() - display.getStrWidth(text)) / 2;
  display.drawStr(max(0, x), y, text);
}

void drawBatteryIcon() {
  int x = display.getDisplayWidth() - 18;
  int y = 1;
  int width = 15;
  int height = 7;
  int percent = batteryPercent();

  display.drawFrame(x, y, width - 2, height);
  display.drawBox(x + width - 2, y + 2, 2, height - 4);

  if (percent < 0) {
    display.drawLine(x + 2, y + 2, x + width - 5, y + height - 3);
    display.drawLine(x + width - 5, y + 2, x + 2, y + height - 3);
    return;
  }

  int fillWidth = map(percent, 0, 100, 0, width - 6);
  if (fillWidth > 0) {
    display.drawBox(x + 2, y + 2, fillWidth, height - 4);
  }
}

void drawHeartbeat() {
  bool pulse = lastPacketPulseMs > 0 && millis() - lastPacketPulseMs < HEARTBEAT_FLASH_MS;
  if (pulse) {
    display.drawDisc(5, 5, 3);
    display.drawDisc(10, 5, 3);
    display.drawTriangle(2, 6, 13, 6, 7, 13);
  } else {
    display.drawCircle(5, 5, 3);
    display.drawCircle(10, 5, 3);
    display.drawLine(2, 6, 7, 13);
    display.drawLine(13, 6, 7, 13);
  }
}

void drawStatusScreen() {
  drawHeartbeat();
  drawBatteryIcon();

  display.setFont(u8g2_font_6x10_tf);
  drawCenteredLine(activeLockLabel(), 9);

  display.setFont(u8g2_font_9x15_tf);
  drawCenteredLine(distanceBlock(), 29);

  display.setFont(u8g2_font_6x12_tf);
  drawCenteredLine(currentDistanceFeet >= 0.0 ? cardinalDirection() : heatLabel(), 46);

  bool showMotionAlert = hiderMovingRecently();
  if (hiderAlertPacketsActive()) {
    showMotionAlert = ((millis() / 200) % 2) == 0;
  }

  if (showMotionAlert) {
    display.setFont(u8g2_font_6x10_tf);
    drawCenteredLine("MOTION ALERT!", 62);
  }
}

void drawGpsTargetScreen() {
  const int centerX = 64;
  const int centerY = 34;
  const int radius = 24;
  double radians = currentBearingDeg * DEG_TO_RAD;
  int targetX = centerX + (int)(sin(radians) * (radius - 4));
  int targetY = centerY - (int)(cos(radians) * (radius - 4));

  drawHeartbeat();
  drawBatteryIcon();
  display.setFont(u8g2_font_5x8_tf);
  display.drawStr(centerX - 2, 7, "N");
  display.drawCircle(centerX, centerY, radius);
  display.drawLine(centerX, centerY, targetX, targetY);
  display.drawDisc(centerX, centerY, 2);
  display.drawDisc(targetX, targetY, 4);
  display.drawCircle(targetX, targetY, 6);
  display.drawStr(2, 62, distanceBlock());
}

void drawSignalHuntScreen() {
  int percent = proximityPercent();
  int activeBars = map(percent, 0, 100, 0, 10);

  drawHeartbeat();
  drawBatteryIcon();

  for (int i = 0; i < 10; i++) {
    int height = 4 + i * 4;
    int x = 13 + i * 10;
    int y = 48 - height;
    display.drawFrame(x, y, 7, height);
    if (i < activeBars) {
      display.drawBox(x + 1, y + 1, 5, height - 2);
    }
  }

  display.setFont(u8g2_font_6x10_tf);
  drawCenteredLine(heatLabel(), 61);
}

void drawHuntScreen() {
  if (currentDistanceFeet >= 0.0) {
    drawGpsTargetScreen();
  } else {
    drawSignalHuntScreen();
  }
}

void updateDisplay() {
  unsigned long intervalMs = hiderAlertPacketsActive() ? 100 : DISPLAY_INTERVAL_MS;
  if (!displayReady || millis() - lastDisplayMs < intervalMs) {
    return;
  }
  lastDisplayMs = millis();

  display.clearBuffer();
  display.setFont(u8g2_font_6x12_tf);
  if (screenTwo) {
    drawHuntScreen();
  } else {
    drawStatusScreen();
  }
  display.sendBuffer();
}

void checkUserButton() {
  bool reading = digitalRead(USER_BUTTON);

  if (reading != lastButtonReading) {
    lastButtonChangeMs = millis();
    lastButtonReading = reading;
  }

  if (millis() - lastButtonChangeMs < BUTTON_DEBOUNCE_MS) {
    return;
  }

  if (reading != buttonState) {
    buttonState = reading;
    if (buttonState == LOW) {
      screenTwo = !screenTwo;
      lastDisplayMs = 0;
    }
  }
}

void setupBle() {
  BLEDevice::init(BLE_SEEKER_NAME);
  bleScan = BLEDevice::getScan();
  bleScan->setAdvertisedDeviceCallbacks(&hiderBleCallbacks, false);
  bleScan->setActiveScan(false);
  bleScan->setInterval(80);
  bleScan->setWindow(40);
  bleReady = true;
}

void scanBle() {
  if (!bleReady || millis() - lastBleScanMs < BLE_SCAN_INTERVAL_MS) {
    return;
  }

  lastBleScanMs = millis();
  bleScan->start(BLE_SCAN_SECONDS, false);
  bleScan->clearResults();
}

String csvField(const String &packet, int fieldNumber) {
  int start = 0;

  for (int field = 0; field < fieldNumber; field++) {
    start = packet.indexOf(',', start);
    if (start < 0) {
      return "";
    }
    start++;
  }

  int end = packet.indexOf(',', start);
  return packet.substring(start, end < 0 ? packet.length() : end);
}

bool parseHiderPacket(String packet) {
  packet.trim();
  if (!packet.startsWith("HIDER,")) {
    return false;
  }

  hiderSeq = csvField(packet, 1).toInt();
  hiderFixValid = csvField(packet, 2).toInt() == 1;
  hiderLat = csvField(packet, 3).toFloat();
  hiderLon = csvField(packet, 4).toFloat();
  hiderSats = csvField(packet, 5).toInt();
  hiderHdop = csvField(packet, 6).toFloat();

  hiderMotion = false;
  hiderMotionValid = false;
  hiderBatteryVValid = false;
  hiderBatteryPctValid = false;
  hiderUptimeValid = false;
  bool alertInPacket = false;

  for (int field = 7; field < 16; field++) {
    String value = csvField(packet, field);
    if (value.length() == 0) {
      break;
    }

    if (value == "ALERT") {
      alertInPacket = true;
      hiderMotion = true;
      hiderMotionValid = true;
    } else if (value.startsWith("M=")) {
      hiderMotion = value.substring(2).toInt() == 1;
      hiderMotionValid = true;
    } else if (value.startsWith("BV=")) {
      hiderBatteryV = value.substring(3).toFloat();
      hiderBatteryVValid = true;
    } else if (value.startsWith("BP=")) {
      hiderBatteryPct = value.substring(3).toInt();
      hiderBatteryPctValid = true;
    } else if (value.startsWith("UP=")) {
      hiderUptimeMs = strtoul(value.substring(3).c_str(), nullptr, 10);
      hiderUptimeValid = true;
    }
  }

  if (alertInPacket) {
    lastHiderAlertMs = millis();
  }
  lastHiderPacketMs = millis();
  return true;
}

void setupLoRa() {
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);

  int state = radio.begin(LORA_FREQ_MHZ);
  if (state == RADIOLIB_ERR_NONE) {
    radio.setBandwidth(125.0);
    radio.setSpreadingFactor(7);
    radio.setCodingRate(7);
    radio.setSyncWord(0x12);
    radio.setPreambleLength(8);
    radio.setCRC(false);
    radio.explicitHeader();
    radio.setPacketReceivedAction(onPacketReceived);
    radio.startReceive();
    radioReady = true;
  }
}

void readLoRa() {
  if (!radioReady || !packetReceived) {
    return;
  }

  packetReceived = false;
  String packet;
  int state = radio.readData(packet);

  if (state == RADIOLIB_ERR_NONE) {
    bool parsed = parseHiderPacket(packet);
    hiderRssi = radio.getRSSI();
    hiderSnr = radio.getSNR();
    hiderRssiValid = true;
    hiderSnrValid = true;
    updateHuntMetrics();
    if (parsed) {
      lastPacketPulseMs = millis();
      publishTelemetry();
    }
  }

  radio.startReceive();
}

void publishTelemetry() {
  if (!hiderFixValid || !seekerGpsHasFix()) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED || !mqttClient.connected()) {
    Serial.println("MQTT publish skipped: not connected");
    return;
  }

  JsonDocument doc;
  doc["device"] = "seeker01";
  doc["source"] = "mqtt";
  doc["mode"] = "lora_gps";
  doc["hider_lat"] = hiderLat;
  doc["hider_lon"] = hiderLon;
  doc["seeker_lat"] = gps.location.lat();
  doc["seeker_lon"] = gps.location.lng();
  doc["rssi"] = hiderRssi;
  doc["snr"] = hiderSnr;
  if (bleSeenRecently() && hiderBleRssiValid) {
    doc["ble_rssi"] = hiderBleRssi;
  }
  if (gps.satellites.isValid()) {
    doc["gps_satellites"] = gps.satellites.value();
  }
  if (gps.hdop.isValid()) {
    doc["gps_hdop"] = gps.hdop.hdop();
  }
  if (gps.speed.isValid()) {
    doc["gps_speed_mps"] = gps.speed.mps();
  }
  if (gps.course.isValid()) {
    doc["gps_course_deg"] = gps.course.deg();
  }
  if (hiderMotionValid) {
    doc["hider_motion"] = hiderMotion;
    doc["motion_alert"] = hiderMotion;
  }
  if (pmuReady && pmu.isBatteryConnect()) {
    doc["battery_v"] = pmu.getBattVoltage() / 1000.0;
    int percent = batteryPercent();
    if (percent >= 0) {
      doc["battery_pct"] = percent;
    }
  }
  if (hiderBatteryVValid) {
    doc["hider_battery_v"] = hiderBatteryV;
  }
  if (hiderBatteryPctValid) {
    doc["hider_battery_pct"] = hiderBatteryPct;
  }
  if (hiderUptimeValid) {
    doc["hider_uptime_ms"] = hiderUptimeMs;
  }
  doc["packet_seq"] = hiderSeq;
  doc["packet_age_ms"] = millis() - lastHiderPacketMs;
  doc["uptime_ms"] = millis();

  char payload[1024];
  size_t length = serializeJson(doc, payload, sizeof(payload));
  bool ok = mqttClient.publish(MQTT_TOPIC, payload, length);

  if (ok) {
    if (millis() - lastMqttSuccessLogMs >= MQTT_SUCCESS_LOG_MS) {
      Serial.print("MQTT telemetry published, seq=");
      Serial.println(hiderSeq);
      lastMqttSuccessLogMs = millis();
    }
  } else {
    Serial.println("MQTT publish failed");
  }
}

void printJsonNumberOrNull(bool valid, double value, int decimals) {
  if (valid) {
    Serial.print(value, decimals);
  } else {
    Serial.print("null");
  }
}

void printJsonUnsignedOrNull(bool valid, unsigned long value) {
  if (valid) {
    Serial.print(value);
  } else {
    Serial.print("null");
  }
}

void printLocationJson(const char *name,
                       const char *device,
                       const char *source,
                       bool fix,
                       double lat,
                       double lon,
                       unsigned long sats,
                       bool hdopValid,
                       double hdop,
                       bool rssiValid,
                       float rssi,
                       unsigned long seq,
                       bool ageValid,
                       unsigned long ageMs) {
  Serial.print("{\"type\":\"location\",\"name\":\"");
  Serial.print(name);
  Serial.print("\",\"device\":\"");
  Serial.print(device);
  Serial.print("\",\"source\":\"");
  Serial.print(source);
  Serial.print("\",\"fix\":");
  Serial.print(fix ? "true" : "false");
  Serial.print(",\"lat\":");
  printJsonNumberOrNull(fix, lat, 6);
  Serial.print(",\"lon\":");
  printJsonNumberOrNull(fix, lon, 6);
  Serial.print(",\"sats\":");
  Serial.print(sats);
  Serial.print(",\"hdop\":");
  printJsonNumberOrNull(hdopValid, hdop, 2);
  Serial.print(",\"rssi\":");
  printJsonNumberOrNull(rssiValid, rssi, 1);
  Serial.print(",\"seq\":");
  Serial.print(seq);
  Serial.print(",\"age_ms\":");
  printJsonUnsignedOrNull(ageValid, ageMs);
  Serial.print(",\"millis\":");
  Serial.print(millis());
  Serial.println("}");
}

void printStatusJson() {
  bool seekerAgeValid = gps.location.isValid();
  bool tagSeen = lastHiderPacketMs > 0;
  unsigned long hiderAgeMs = tagSeen ? millis() - lastHiderPacketMs : 0;

  printLocationJson("Seeker", "SEEKER01", "gps",
                    seekerGpsHasFix(), gps.location.lat(), gps.location.lng(),
                    seekerSatCount(), seekerHdopValid(), gps.hdop.hdop(),
                    false, 0.0, 0, seekerAgeValid,
                    seekerAgeValid ? gps.location.age() : 0);

  printLocationJson("Hider", "HIDER01", "lora",
                    hiderFixValid, hiderLat, hiderLon, hiderSats,
                    tagSeen, hiderHdop, hiderRssiValid, hiderRssi,
                    hiderSeq, tagSeen, hiderAgeMs);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  setupPower();
  setupDisplay();
  pinMode(USER_BUTTON, INPUT_PULLUP);
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  setupLoRa();
  setupBle();
  setupMqtt();
}

void loop() {
  checkUserButton();
  maintainWiFi();
  maintainMqtt();
  readGps();
  readLoRa();
  scanBle();
  updateDisplay();

  if (millis() - lastStatusMs >= STATUS_INTERVAL_MS) {
    printStatusJson();
    lastStatusMs = millis();
  }
}
