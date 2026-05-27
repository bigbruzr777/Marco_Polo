#include <Arduino.h>
#include <AltSoftSerial.h>
#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>

// MarcoPolo GPS Hider.
// E32:
//   E32 TXD -> Arduino D10
//   Arduino D11 -> resistor divider -> E32 RXD
//   E32 AUX -> Arduino D4
//   E32 M0  -> GND
//   E32 M1  -> GND
// GPS:
//   GPS VCC -> 5V rail
//   GPS GND -> GND rail
//   GPS TX  -> Arduino D8
//   GPS RX is not connected
// Serial:
//   USB Serial Monitor: 115200 baud
//   E32 UART:           9600 baud
//   GPS UART:           9600 baud
SoftwareSerial loraSerial(10, 11);
AltSoftSerial gpsSerial;
TinyGPSPlus gps;

const int LORA_AUX_PIN = 4;
const unsigned long SEND_INTERVAL_MS = 2000;
const unsigned long GPS_FIX_MAX_AGE_MS = 5000;

unsigned long lastSendMs = 0;
unsigned long packetSeq = 1;

void flashHiderLed() {
  pinMode(LED_BUILTIN, OUTPUT);

  unsigned long startMs = millis();
  while (millis() - startMs < 5000) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
  }
}

void readGps() {
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }
}

bool gpsHasFreshFix() {
  return gps.location.isValid() && gps.location.age() < GPS_FIX_MAX_AGE_MS;
}

unsigned long gpsSatCount() {
  if (gps.satellites.isValid()) {
    return gps.satellites.value();
  }

  return 0;
}

double gpsHdop() {
  if (gps.hdop.isValid()) {
    return gps.hdop.hdop();
  }

  return 0.0;
}

String buildHiderPacket() {
  bool hasFix = gpsHasFreshFix();
  unsigned long sats = gpsSatCount();

  if (!hasFix) {
    return "HIDER," + String(packetSeq) + ",0,0,0," + String(sats) + ",0";
  }

  return "HIDER," + String(packetSeq) +
         ",1," + String(gps.location.lat(), 6) +
         "," + String(gps.location.lng(), 6) +
         "," + String(sats) +
         "," + String(gpsHdop(), 2);
}

void printGpsDebug(String packet) {
  bool hasFix = gpsHasFreshFix();

  Serial.println("--- MarcoPolo Hider GPS ---");
  Serial.print("GPS fix status: ");
  Serial.println(hasFix ? "FIX" : "NOFIX");
  Serial.print("GPS chars/sentences/checksum_fail: ");
  Serial.print(gps.charsProcessed());
  Serial.print("/");
  Serial.print(gps.sentencesWithFix());
  Serial.print("/");
  Serial.println(gps.failedChecksum());

  if (hasFix) {
    Serial.print("Lat/Lon: ");
    Serial.print(gps.location.lat(), 6);
    Serial.print(", ");
    Serial.println(gps.location.lng(), 6);
  } else {
    Serial.println("Lat/Lon: 0, 0");
  }

  Serial.print("GPS sats: ");
  Serial.println(gpsSatCount());
  Serial.print("GPS hdop: ");
  Serial.println(gpsHdop(), 2);
  Serial.print("E32 AUX: ");
  Serial.println(digitalRead(LORA_AUX_PIN) == HIGH ? "HIGH/READY" : "LOW/BUSY");
  Serial.print("Packet sent: ");
  Serial.println(packet);
  Serial.println("---------------------------");
}

void setup() {
  flashHiderLed();

  Serial.begin(115200);
  loraSerial.begin(9600);
  gpsSerial.begin(9600);
  pinMode(LORA_AUX_PIN, INPUT_PULLUP);

  Serial.println("MarcoPolo Hider GPS test starting...");
  Serial.println("Reading GPS on AltSoftSerial RX=D8.");
  Serial.println("Sending one HIDER GPS packet every 2 seconds.");
}

void loop() {
  readGps();

  unsigned long now = millis();
  if (now - lastSendMs >= SEND_INTERVAL_MS) {
    String packet = buildHiderPacket();

    loraSerial.println(packet);
    printGpsDebug(packet);
    packetSeq++;

    lastSendMs = now;
  }
}
