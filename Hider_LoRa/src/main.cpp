#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <RadioLib.h>
#include <SensorQMI8658.hpp>
#include <TinyGPSPlus.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <XPowersLib.h>

const int I2C_SDA = 17;
const int I2C_SCL = 18;
const int PMU_SDA = 42;
const int PMU_SCL = 41;

const int GPS_RX = 9;
const int GPS_TX = 8;
const int GPS_EN = 7;
const int BOOT_BUTTON = 0;

const int LORA_SCK = 12;
const int LORA_MISO = 13;
const int LORA_MOSI = 11;
const int LORA_CS = 10;
const int LORA_RST = 5;
const int LORA_DIO1 = 1;
const int LORA_BUSY = 4;
const int IMU_CS = 34;
const int IMU_MOSI = 35;
const int IMU_SCK = 36;
const int IMU_MISO = 37;

const float LORA_FREQ_MHZ = 915.0;
const char *BLE_DEVICE_NAME = "MP-HIDER01";
const char *BLE_SERVICE_UUID = "b7a2c91d-7d8d-4c44-9cb1-4dd5e2fd0101";
const unsigned long SEND_INTERVAL_MS = 2000;
const unsigned long MOVING_SEND_INTERVAL_MS = 1000;
const unsigned long GPS_FIX_MAX_AGE_MS = 5000;
const unsigned long BUTTON_DEBOUNCE_MS = 50;
const unsigned long MOTION_STILL_MS = 8000;
const unsigned long MOTION_SAMPLE_MS = 50;
const float MOTION_ACCEL_DELTA = 0.03;
const float MOTION_GYRO_DPS = 5.0;
const int BATTERY_ICON_WIDTH = 16;
const int BATTERY_ICON_HEIGHT = 8;

TinyGPSPlus gps;
HardwareSerial gpsSerial(1);
TwoWire pmuWire = TwoWire(1);
SPIClass loraSpi(FSPI);
SPIClass imuSpi(HSPI);
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, loraSpi);
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
XPowersPMU pmu;
SensorQMI8658 imu;
IMUdata acc;
IMUdata gyr;

unsigned long lastSendMs = 0;
unsigned long lastDisplayMs = 0;
unsigned long lastButtonChangeMs = 0;
unsigned long lastMotionSampleMs = 0;
unsigned long lastMotionMs = 0;
unsigned long packetSeq = 1;
bool radioReady = false;
bool displayReady = false;
bool pmuReady = false;
bool imuReady = false;
bool imuBaselineReady = false;
bool forceSendNow = false;
bool gpsEnabled = true;
bool lastBootReading = HIGH;
bool bootButtonState = HIGH;
String lastRadioStatus = "not started";
float lastAccelMag = 0.0;
float lastAccX = 0.0;
float lastAccY = 0.0;
float lastAccZ = 0.0;

void enablePowerRails() {
  if (!pmu.begin(pmuWire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL)) {
    Serial.println("PMU not found");
    return;
  }
  pmuReady = true;

  pmu.setALDO1Voltage(3300);
  pmu.enableALDO1();
  pmu.setALDO2Voltage(3300);
  pmu.enableALDO2();
  pmu.setALDO3Voltage(3300);
  pmu.enableALDO3();
  pmu.setALDO4Voltage(3300);
  pmu.enableALDO4();
  pmu.enableBattVoltageMeasure();
  pmu.enableVbusVoltageMeasure();

  pmu.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
}

void setGpsPower(bool enabled) {
  gpsEnabled = enabled;

  if (enabled) {
    if (pmuReady) {
      pmu.setALDO4Voltage(3300);
      pmu.enableALDO4();
    }
    digitalWrite(GPS_EN, HIGH);
    gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
    gps = TinyGPSPlus();
  } else {
    gpsSerial.end();
    digitalWrite(GPS_EN, LOW);
    if (pmuReady) {
      pmu.disableALDO4();
    }
    gps = TinyGPSPlus();
  }
}

bool i2cDevicePresent(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void setupDisplay() {
  delay(250);

  bool found3c = i2cDevicePresent(0x3C);
  bool found3d = i2cDevicePresent(0x3D);

  if (found3d) {
    display.setI2CAddress(0x3D * 2);
  } else {
    display.setI2CAddress(0x3C * 2);
  }

  display.begin();
  display.setPowerSave(0);
  display.setContrast(255);
  display.setDisplayRotation(U8G2_R1);
  displayReady = found3c || found3d;
}

void readGps() {
  if (!gpsEnabled) {
    return;
  }

  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }
}

bool gpsHasFreshFix() {
  return gps.location.isValid() && gps.location.age() < GPS_FIX_MAX_AGE_MS;
}

unsigned long gpsSatCount() {
  if (!gpsEnabled) {
    return 0;
  }

  if (gps.satellites.isValid()) {
    return gps.satellites.value();
  }

  return 0;
}

double gpsHdop() {
  if (!gpsEnabled) {
    return 99.99;
  }

  if (gps.hdop.isValid()) {
    return gps.hdop.hdop();
  }

  return 99.99;
}

bool movingAlertActive() {
  return lastMotionMs > 0 && millis() - lastMotionMs < MOTION_STILL_MS;
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

void drawBatteryIcon(int x, int y) {
  if (!pmuReady) {
    return;
  }

  int percent = batteryPercent();
  bool hasBattery = percent >= 0;
  bool charging = pmu.isCharging() || pmu.isVbusIn();

  display.drawFrame(x, y, BATTERY_ICON_WIDTH - 2, BATTERY_ICON_HEIGHT);
  display.drawBox(x + BATTERY_ICON_WIDTH - 2, y + 2, 2, BATTERY_ICON_HEIGHT - 4);

  if (!hasBattery) {
    display.drawLine(x + 2, y + 2, x + BATTERY_ICON_WIDTH - 5, y + BATTERY_ICON_HEIGHT - 3);
    display.drawLine(x + BATTERY_ICON_WIDTH - 5, y + 2, x + 2, y + BATTERY_ICON_HEIGHT - 3);
    return;
  }

  int fillWidth = map(percent, 0, 100, 0, BATTERY_ICON_WIDTH - 6);
  if (fillWidth > 0) {
    display.drawBox(x + 2, y + 2, fillWidth, BATTERY_ICON_HEIGHT - 4);
  }

  if (charging) {
    display.setDrawColor(2);
    display.drawLine(x + 7, y + 1, x + 5, y + 4);
    display.drawLine(x + 5, y + 4, x + 8, y + 4);
    display.drawLine(x + 8, y + 4, x + 6, y + 7);
    display.setDrawColor(1);
  }
}

String buildHiderPacket() {
  bool moving = movingAlertActive();
  String packet;
  packet.reserve(128);

  if (!gpsEnabled || !gpsHasFreshFix()) {
    packet = "HIDER," + String(packetSeq) +
             ",0,0,0," + String(gpsSatCount()) +
             "," + String(gpsHdop(), 2);
  } else {
    packet = "HIDER," + String(packetSeq) +
             ",1," + String(gps.location.lat(), 6) +
             "," + String(gps.location.lng(), 6) +
             "," + String(gpsSatCount()) +
             "," + String(gpsHdop(), 2);
  }

  if (moving) {
    packet += ",ALERT";
  }

  packet += moving ? ",M=1" : ",M=0";
  if (pmuReady && pmu.isBatteryConnect()) {
    packet += ",BV=" + String(pmu.getBattVoltage() / 1000.0, 3);
    int percent = batteryPercent();
    if (percent >= 0) {
      packet += ",BP=" + String(percent);
    }
  }
  packet += ",UP=" + String(millis());
  return packet;
}

void drawCenteredText(const char *line1, const char *line2, const char *line3) {
  if (!displayReady) {
    return;
  }

  display.clearBuffer();
  int width = display.getDisplayWidth();
  int height = display.getDisplayHeight();

  display.drawFrame(0, 0, width, height);
  drawBatteryIcon(width - BATTERY_ICON_WIDTH - 3, 3);
  display.setFont(u8g2_font_6x12_tf);

  int line1Width = display.getStrWidth(line1);
  int line2Width = display.getStrWidth(line2);
  int line3Width = display.getStrWidth(line3);
  int firstY = (height - 30) / 2 + 10;

  display.drawStr((width - line1Width) / 2, firstY, line1);
  display.drawStr((width - line2Width) / 2, firstY + 14, line2);
  display.drawStr((width - line3Width) / 2, firstY + 28, line3);
  display.sendBuffer();
}

void updateDisplay() {
  if (millis() - lastDisplayMs < 500) {
    return;
  }
  lastDisplayMs = millis();

  char satLine[12];
  snprintf(satLine, sizeof(satLine), "Sat %lu", gpsSatCount());

  if (!gpsEnabled) {
    drawCenteredText("Tag 1", "GPS OFF", satLine);
  } else if (gps.charsProcessed() == 0) {
    drawCenteredText("Tag 1", "No Data", satLine);
  } else if (gpsHasFreshFix()) {
    drawCenteredText("Tag 1", "GPS Fix", satLine);
  } else {
    drawCenteredText("Tag 1", "GPS No Fix", satLine);
  }
}

void checkBootButton() {
  bool reading = digitalRead(BOOT_BUTTON);

  if (reading != lastBootReading) {
    lastButtonChangeMs = millis();
    lastBootReading = reading;
  }

  if (millis() - lastButtonChangeMs < BUTTON_DEBOUNCE_MS) {
    return;
  }

  if (reading != bootButtonState) {
    bootButtonState = reading;

    if (bootButtonState == LOW) {
      setGpsPower(!gpsEnabled);
      Serial.print("BOOT button: GPS ");
      Serial.println(gpsEnabled ? "ON" : "OFF");
    }
  }
}

void setupRadio() {
  loraSpi.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);

  int state = radio.begin(LORA_FREQ_MHZ);
  if (state == RADIOLIB_ERR_NONE) {
    radio.setBandwidth(125.0);
    radio.setSpreadingFactor(7);
    radio.setCodingRate(7);
    radio.setSyncWord(0x12);
    radio.setPreambleLength(8);
    radio.setCRC(0);
    radio.explicitHeader();
    radio.setOutputPower(17);
    radioReady = true;
    lastRadioStatus = "ready";
  } else {
    radioReady = false;
    lastRadioStatus = "err " + String(state);
  }
}

void setupImu() {
  imuReady = imu.begin(imuSpi, IMU_CS, IMU_MOSI, IMU_MISO, IMU_SCK);
  if (!imuReady) {
    Serial.println("IMU: not found");
    return;
  }

  imu.configAccelerometer(SensorQMI8658::ACC_RANGE_4G,
                          SensorQMI8658::ACC_ODR_125Hz,
                          SensorQMI8658::LPF_MODE_0);
  imu.configGyroscope(SensorQMI8658::GYR_RANGE_256DPS,
                      SensorQMI8658::GYR_ODR_112_1Hz,
                      SensorQMI8658::LPF_MODE_3);
  imu.enableAccelerometer();
  imu.enableGyroscope();
  Serial.println("IMU: ready");
}

void readMotion() {
  if (!imuReady || millis() - lastMotionSampleMs < MOTION_SAMPLE_MS) {
    return;
  }
  lastMotionSampleMs = millis();

  if (!imu.getAccelerometer(acc.x, acc.y, acc.z)) {
    return;
  }

  float accelMag = sqrt(acc.x * acc.x + acc.y * acc.y + acc.z * acc.z);
  float gyroMotion = 0.0;
  if (imu.getGyroscope(gyr.x, gyr.y, gyr.z)) {
    gyroMotion = max(max(fabs(gyr.x), fabs(gyr.y)), fabs(gyr.z));
  }

  if (!imuBaselineReady) {
    lastAccelMag = accelMag;
    lastAccX = acc.x;
    lastAccY = acc.y;
    lastAccZ = acc.z;
    imuBaselineReady = true;
    return;
  }

  float axisDelta = sqrt((acc.x - lastAccX) * (acc.x - lastAccX) +
                         (acc.y - lastAccY) * (acc.y - lastAccY) +
                         (acc.z - lastAccZ) * (acc.z - lastAccZ));

  bool wasMoving = movingAlertActive();
  if (axisDelta > MOTION_ACCEL_DELTA ||
      fabs(accelMag - lastAccelMag) > MOTION_ACCEL_DELTA ||
      gyroMotion > MOTION_GYRO_DPS) {
    lastMotionMs = millis();
    if (!wasMoving) {
      forceSendNow = true;
      Serial.println("Motion: ALERT");
    }
  }

  lastAccelMag = lastAccelMag * 0.85 + accelMag * 0.15;
  lastAccX = lastAccX * 0.85 + acc.x * 0.15;
  lastAccY = lastAccY * 0.85 + acc.y * 0.15;
  lastAccZ = lastAccZ * 0.85 + acc.z * 0.15;
}

void setupBleBeacon() {
  BLEDevice::init(BLE_DEVICE_NAME);

  BLEAdvertisementData advertisement;
  advertisement.setName(BLE_DEVICE_NAME);
  advertisement.setCompleteServices(BLEUUID(BLE_SERVICE_UUID));
  advertisement.setManufacturerData("MarcoPolo,HIDER01");

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->setAdvertisementData(advertisement);
  advertising->setScanResponse(false);
  advertising->setMinInterval(0x100);
  advertising->setMaxInterval(0x200);
  BLEDevice::startAdvertising();

  Serial.print("BLE: advertising ");
  Serial.println(BLE_DEVICE_NAME);
}

void sendPacket() {
  String packet = buildHiderPacket();

  if (radioReady) {
    int state = radio.transmit(packet);
    lastRadioStatus = state == RADIOLIB_ERR_NONE ? "sent" : "tx err " + String(state);
  }

  Serial.print("Packet: ");
  Serial.println(packet);
  Serial.print("GPS: chars=");
  Serial.print(gps.charsProcessed());
  Serial.print(" sats=");
  Serial.print(gpsSatCount());
  Serial.print(" hdop=");
  Serial.println(gpsHdop(), 2);
  Serial.print("LoRa: ");
  Serial.println(lastRadioStatus);
  if (pmuReady) {
    Serial.print("Battery: ");
    Serial.print(batteryPercent());
    Serial.print("% ");
    Serial.print(pmu.getBattVoltage());
    Serial.print("mV charging=");
    Serial.println(pmu.isCharging() ? "yes" : "no");
  }

  packetSeq++;
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(I2C_SDA, I2C_SCL);
  enablePowerRails();

  setupDisplay();
  drawCenteredText("Tag 1", "Starting", "Sat 0");

  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  pinMode(GPS_EN, OUTPUT);
  setGpsPower(true);
  setupImu();
  setupRadio();
  setupBleBeacon();

  Serial.println("MarcoPolo T-Beam Supreme Hider");
}

void loop() {
  checkBootButton();
  readGps();
  readMotion();
  updateDisplay();

  unsigned long now = millis();
  unsigned long interval = movingAlertActive() ? MOVING_SEND_INTERVAL_MS : SEND_INTERVAL_MS;
  if (forceSendNow || now - lastSendMs >= interval) {
    sendPacket();
    lastSendMs = now;
    forceSendNow = false;
  }
}
