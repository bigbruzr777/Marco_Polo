#include <Arduino.h>
#include <TinyGPSPlus.h>

// Nano 33 BLE Sense Hider.
// Serial1 RX D0 <- GPS TX. Serial1 TX D1 -> E32 RX.
// E32 M0/M1 -> GND. E32 TX and AUX not connected.
TinyGPSPlus gps;

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
  while (Serial1.available() > 0) {
    gps.encode(Serial1.read());
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

  return 99.99;
}

String buildHiderPacket() {
  bool hasFix = gpsHasFreshFix();
  unsigned long sats = gpsSatCount();

  if (!hasFix) {
    return "HIDER," + String(packetSeq) +
           ",0,0,0," + String(sats) +
           "," + String(gpsHdop(), 2);
  }

  return "HIDER," + String(packetSeq) +
         ",1," + String(gps.location.lat(), 6) +
         "," + String(gps.location.lng(), 6) +
         "," + String(sats) +
         "," + String(gpsHdop(), 2);
}

void printGpsDebug(String packet) {
  bool hasFix = gpsHasFreshFix();

  Serial.println("--- MarcoPolo Nano Hider GPS ---");
  Serial.print("GPS fix status: ");
  Serial.println(hasFix ? "FIX" : "NOFIX");
  Serial.print("GPS chars/sentences/checksum_fail: ");
  Serial.print(gps.charsProcessed());
  Serial.print("/");
  Serial.print(gps.sentencesWithFix());
  Serial.print("/");
  Serial.println(gps.failedChecksum());
  Serial.print("GPS sats: ");
  Serial.println(gpsSatCount());
  Serial.print("GPS hdop: ");
  Serial.println(gpsHdop(), 2);

  if (hasFix) {
    Serial.print("Lat/Lon: ");
    Serial.print(gps.location.lat(), 6);
    Serial.print(", ");
    Serial.println(gps.location.lng(), 6);
  } else {
    Serial.println("Lat/Lon: 0, 0");
  }

  Serial.print("Packet sent: ");
  Serial.println(packet);
  Serial.println("-------------------------------");
}

void setup() {
  flashHiderLed();

  Serial.begin(115200);
  Serial1.begin(9600);

  Serial.println("MarcoPolo Nano Hider starting...");
  Serial.println("GPS RX on D0. E32 TX out on D1.");
}

void loop() {
  readGps();

  unsigned long now = millis();
  if (now - lastSendMs >= SEND_INTERVAL_MS) {
    String packet = buildHiderPacket();

    Serial1.println(packet);
    Serial1.flush();
    printGpsDebug(packet);
    packetSeq++;

    lastSendMs = now;
  }
}
