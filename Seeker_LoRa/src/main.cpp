#include <Arduino.h>
#include <AltSoftSerial.h>
#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>

// MarcoPolo GPS Seeker.
// E32: TXD->D10, D11->divider->RXD, AUX->D4, M0/M1->GND.
// GPS: VCC->5V, GND->GND, TX->D8, RX not connected.
SoftwareSerial loraSerial(10, 11);
AltSoftSerial gpsSerial;
TinyGPSPlus gps;

const unsigned long STATUS_INTERVAL_MS = 2000;
const unsigned long GPS_FIX_MAX_AGE_MS = 5000;
const unsigned long LORA_BUFFER_LIMIT = 90;

String incomingLoRa = "";

bool hiderFixValid = false;
double hiderLat = 0.0;
double hiderLon = 0.0;
unsigned long hiderSats = 0;
double hiderHdop = 0.0;
unsigned long hiderSeq = 0;
unsigned long lastHiderPacketMs = 0;
unsigned long lastStatusMs = 0;

void readGps() {
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }
}

bool seekerGpsHasFreshFix() {
  return gps.location.isValid() && gps.location.age() < GPS_FIX_MAX_AGE_MS;
}

unsigned long seekerSatCount() {
  if (gps.satellites.isValid()) {
    return gps.satellites.value();
  }

  return 0;
}

bool seekerHdopValid() {
  return gps.hdop.isValid();
}

String csvField(String packet, int fieldNumber) {
  int fieldStart = 0;

  for (int currentField = 0; currentField < fieldNumber; currentField++) {
    fieldStart = packet.indexOf(',', fieldStart);

    if (fieldStart < 0) {
      return "";
    }

    fieldStart++;
  }

  int fieldEnd = packet.indexOf(',', fieldStart);
  if (fieldEnd < 0) {
    fieldEnd = packet.length();
  }

  return packet.substring(fieldStart, fieldEnd);
}

void parseHiderPacket(String packet) {
  packet.trim();

  if (!packet.startsWith("HIDER,")) {
    return;
  }

  hiderSeq = csvField(packet, 1).toInt();
  hiderFixValid = csvField(packet, 2).toInt() == 1;
  hiderLat = csvField(packet, 3).toFloat();
  hiderLon = csvField(packet, 4).toFloat();
  hiderSats = csvField(packet, 5).toInt();
  hiderHdop = csvField(packet, 6).toFloat();
  lastHiderPacketMs = millis();
}

void readLoRa() {
  while (loraSerial.available() > 0) {
    char receivedChar = loraSerial.read();

    if (receivedChar == '\n') {
      parseHiderPacket(incomingLoRa);
      incomingLoRa = "";
    } else if (receivedChar != '\r') {
      incomingLoRa += receivedChar;
    }

    if (incomingLoRa.length() > LORA_BUFFER_LIMIT) {
      incomingLoRa = "";
    }
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
  Serial.print(",\"rssi\":null,\"seq\":");
  Serial.print(seq);
  Serial.print(",\"age_ms\":");
  printJsonUnsignedOrNull(ageValid, ageMs);
  Serial.print(",\"millis\":");
  Serial.print(millis());
  Serial.println("}");
}

void printStatusJson() {
  bool seekerFix = seekerGpsHasFreshFix();
  bool seekerAgeValid = gps.location.isValid();
  unsigned long seekerAgeMs = seekerAgeValid ? gps.location.age() : 0;

  printLocationJson("Seeker",
                    "SEEKER01",
                    "gps",
                    seekerFix,
                    gps.location.lat(),
                    gps.location.lng(),
                    seekerSatCount(),
                    seekerHdopValid(),
                    gps.hdop.hdop(),
                    0,
                    seekerAgeValid,
                    seekerAgeMs);

  bool hiderSeen = lastHiderPacketMs > 0;
  unsigned long hiderAgeMs = hiderSeen ? millis() - lastHiderPacketMs : 0;

  printLocationJson("Hider",
                    "HIDER01",
                    "lora",
                    hiderFixValid,
                    hiderLat,
                    hiderLon,
                    hiderSats,
                    hiderSeen,
                    hiderHdop,
                    hiderSeq,
                    hiderSeen,
                    hiderAgeMs);
}

void setup() {
  Serial.begin(115200);
  loraSerial.begin(9600);
  gpsSerial.begin(9600);
}

void loop() {
  readGps();
  readLoRa();

  unsigned long now = millis();
  if (now - lastStatusMs >= STATUS_INTERVAL_MS) {
    printStatusJson();
    lastStatusMs = now;
  }
}
