#include <Arduino.h>
#include <SoftwareSerial.h>

// MarcoPolo Phase 1 - Seeker two-way LoRa heartbeat test
//
// Wiring for the EBYTE E32 module:
//   E32 TXD -> Arduino D10
//   Arduino D11 -> voltage divider -> E32 RXD
//   E32 AUX -> Arduino D4
//   E32 M0  -> GND
//   E32 M1  -> GND
//
// SoftwareSerial pin order is: RX pin, TX pin.
// D10 receives data from the LoRa module.
// D11 sends data to the LoRa module.
SoftwareSerial loraSerial(10, 11);

const int LORA_AUX_PIN = 4;

// Both boards use the same 4 second cycle.
// Hider sends near the start of the cycle.
// Seeker sends about 2 seconds later.
const unsigned long CYCLE_MS = 4000;
const unsigned long MY_TX_OFFSET_MS = 2000;
const unsigned long TX_WINDOW_MS = 250;

String incomingPacket = "";
unsigned long packetNumber = 1;
unsigned long lastTxCycle = 999999;
unsigned long lastStatusTime = 0;
unsigned long lastHiderSeq = 0;
unsigned long hiderPacketsHeard = 0;
unsigned long hiderPacketsMissed = 0;
unsigned long lastHiderPacketMs = 0;
int lastAuxState = HIGH;

String auxStateText() {
  if (digitalRead(LORA_AUX_PIN) == HIGH) {
    return "HIGH/READY";
  }

  return "LOW/BUSY";
}

void printPacket(String direction, String packet) {
  Serial.print(direction);
  Serial.print(": ");
  Serial.println(packet);
}

String getFieldValue(String packet, String fieldName) {
  String searchText = fieldName + "=";
  int fieldStart = packet.indexOf(searchText);

  if (fieldStart < 0) {
    return "";
  }

  fieldStart += searchText.length();
  int fieldEnd = packet.indexOf(',', fieldStart);

  if (fieldEnd < 0) {
    fieldEnd = packet.length();
  }

  return packet.substring(fieldStart, fieldEnd);
}

void updateHiderTracking(String packet) {
  if (!packet.startsWith("MP1,HIDER01,BEACON,")) {
    return;
  }

  unsigned long sequenceNumber = getFieldValue(packet, "SEQ").toInt();
  unsigned long hiderMillis = getFieldValue(packet, "MS").toInt();

  hiderPacketsHeard++;
  lastHiderPacketMs = millis();

  if (lastHiderSeq > 0 && sequenceNumber > lastHiderSeq + 1) {
    hiderPacketsMissed += sequenceNumber - lastHiderSeq - 1;
  }

  lastHiderSeq = sequenceNumber;

  Serial.print("TRACK HIDER01 seq=");
  Serial.print(sequenceNumber);
  Serial.print(" heard=");
  Serial.print(hiderPacketsHeard);
  Serial.print(" missed=");
  Serial.print(hiderPacketsMissed);
  Serial.print(" age_ms=0");
  Serial.print(" hider_ms=");
  Serial.print(hiderMillis);
  Serial.println(" rssi=NA");
}

void readLoRaPackets() {
  while (loraSerial.available() > 0) {
    char receivedChar = loraSerial.read();

    if (receivedChar == '\n') {
      String packet = incomingPacket;
      packet.trim();
      incomingPacket = "";

      if (packet.length() > 0) {
        printPacket("RX", packet);
        updateHiderTracking(packet);
      }
    } else {
      incomingPacket += receivedChar;
    }

    if (incomingPacket.length() > 80) {
      Serial.print("RX buffer cleared: ");
      Serial.println(incomingPacket);
      incomingPacket = "";
    }
  }
}

void sendHeartbeat() {
  // MarcoPolo packet format v1:
  //   MP1,SOURCE,TYPE,SEQ=n,MS=n,LAST_HIDER_SEQ=n,FLAGS=text
  String packet = "MP1,SEEKER01,STATUS,SEQ=" + String(packetNumber) +
                  ",MS=" + String(millis()) +
                  ",LAST_HIDER_SEQ=" + String(lastHiderSeq) +
                  ",FLAGS=OK";
  bool auxWasBusy = false;

  Serial.print("TX AUX before: ");
  Serial.println(auxStateText());

  loraSerial.println(packet);

  // Watch AUX briefly after sending. A LOW pulse suggests the E32 accepted work.
  unsigned long watchStart = millis();
  while (millis() - watchStart < 300) {
    readLoRaPackets();

    if (digitalRead(LORA_AUX_PIN) == LOW) {
      auxWasBusy = true;
    }
  }

  printPacket("TX", packet);
  Serial.print("TX AUX after: ");
  Serial.print(auxStateText());
  Serial.print(" busy pulse seen: ");
  Serial.println(auxWasBusy ? "YES" : "NO");

  packetNumber++;
}

void setup() {
  Serial.begin(9600);
  pinMode(LORA_AUX_PIN, INPUT_PULLUP);
  lastAuxState = digitalRead(LORA_AUX_PIN);

  loraSerial.begin(9600);

  Serial.println("MarcoPolo Seeker two-way test starting...");
  Serial.println("Seeker TX slot: 2 seconds after each Hider TX slot.");
  Serial.println("Listening between Seeker TX slots.");
  Serial.print("AUX pin D4 state: ");
  Serial.println(auxStateText());
}

void loop() {
  readLoRaPackets();

  int auxState = digitalRead(LORA_AUX_PIN);
  if (auxState != lastAuxState) {
    Serial.print("AUX changed: ");
    Serial.println(auxStateText());
    lastAuxState = auxState;
  }

  unsigned long now = millis();
  unsigned long cycleNumber = now / CYCLE_MS;
  unsigned long cyclePosition = now % CYCLE_MS;

  bool inMyTxWindow = cyclePosition >= MY_TX_OFFSET_MS &&
                      cyclePosition < (MY_TX_OFFSET_MS + TX_WINDOW_MS);

  if (inMyTxWindow && cycleNumber != lastTxCycle) {
    sendHeartbeat();
    lastTxCycle = cycleNumber;
  }

  if (now - lastStatusTime >= 5000) {
    Serial.print("Listening... AUX=");
    Serial.print(auxStateText());

    if (lastHiderPacketMs > 0) {
      Serial.print(" last_hider_age_ms=");
      Serial.print(now - lastHiderPacketMs);
      Serial.print(" heard=");
      Serial.print(hiderPacketsHeard);
      Serial.print(" missed=");
      Serial.print(hiderPacketsMissed);
    } else {
      Serial.print(" last_hider_age_ms=NA heard=0 missed=0");
    }

    Serial.println(" rssi=NA");
    lastStatusTime = now;
  }
}
