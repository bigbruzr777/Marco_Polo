#include <Arduino.h>
#include <AltSoftSerial.h>
#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>

// MarcoPolo GPS Seeker.
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
const unsigned long STATUS_INTERVAL_MS = 2000;
const unsigned long GPS_FIX_MAX_AGE_MS = 5000;
const unsigned long LORA_BUFFER_LIMIT = 90;

String incomingLoRa = "";
String lastHiderPacket = "none";

bool hiderFixValid = false;
double hiderLat = 0.0;
double hiderLon = 0.0;
unsigned long hiderSats = 0;
double hiderHdop = 0.0;
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

  lastHiderPacket = packet;
  lastHiderPacketMs = millis();

  hiderFixValid = csvField(packet, 1).toInt() == 1;
  hiderLat = csvField(packet, 2).toFloat();
  hiderLon = csvField(packet, 3).toFloat();
  hiderSats = csvField(packet, 4).toInt();
  hiderHdop = csvField(packet, 5).toFloat();
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

void printLocationLine(bool hasFix, double lat, double lon) {
  if (hasFix) {
    Serial.print(lat, 6);
    Serial.print(", ");
    Serial.println(lon, 6);
  } else {
    Serial.println("0, 0");
  }
}

void printStatusBlock() {
  bool seekerFix = seekerGpsHasFreshFix();

  Serial.println("--- MarcoPolo GPS Test ---");
  Serial.print("Seeker GPS: ");
  Serial.println(seekerFix ? "FIX" : "NOFIX");
  Serial.print("Seeker Lat/Lon: ");
  printLocationLine(seekerFix, gps.location.lat(), gps.location.lng());
  Serial.print("Seeker sats: ");
  Serial.println(seekerSatCount());
  Serial.println();

  Serial.print("Hider GPS: ");
  Serial.println(hiderFixValid ? "FIX" : "NOFIX");
  Serial.print("Hider Lat/Lon: ");
  printLocationLine(hiderFixValid, hiderLat, hiderLon);
  Serial.print("Hider sats: ");
  Serial.println(hiderSats);

  Serial.print("Hider packet age: ");
  if (lastHiderPacketMs > 0) {
    Serial.print((millis() - lastHiderPacketMs) / 1000.0, 1);
    Serial.println("s");
  } else {
    Serial.println("NA");
  }

  Serial.print("Hider hdop: ");
  Serial.println(hiderHdop, 2);
  Serial.print("Raw last Hider packet: ");
  Serial.println(lastHiderPacket);
  Serial.println("rssi=NA");
  Serial.println("--------------------------");
}

void setup() {
  Serial.begin(115200);
  loraSerial.begin(9600);
  gpsSerial.begin(9600);
  pinMode(LORA_AUX_PIN, INPUT_PULLUP);

  Serial.println("MarcoPolo Seeker GPS test starting...");
  Serial.println("Reading Seeker GPS on AltSoftSerial RX=D8.");
  Serial.println("Listening for HIDER GPS packets on E32 LoRa.");
}

void loop() {
  readGps();
  readLoRa();

  unsigned long now = millis();
  if (now - lastStatusMs >= STATUS_INTERVAL_MS) {
    printStatusBlock();
    lastStatusMs = now;
  }
}
