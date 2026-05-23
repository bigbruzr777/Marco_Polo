#include <Arduino.h>
#include <SoftwareSerial.h>

// MarcoPolo Phase 1 - Hider two-way LoRa heartbeat test
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
const unsigned long MY_TX_OFFSET_MS = 0;
const unsigned long TX_WINDOW_MS = 250;

String incomingPacket = "";
unsigned long packetNumber = 1;
unsigned long lastTxCycle = 999999;
unsigned long lastStatusTime = 0;
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

void readLoRaPackets() {
  while (loraSerial.available() > 0) {
    char receivedChar = loraSerial.read();

    if (receivedChar == '\n') {
      String packet = incomingPacket;
      packet.trim();
      incomingPacket = "";

      if (packet.length() > 0) {
        printPacket("RX", packet);
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
  String packet = "HIDER01,HEARTBEAT,PKT=" + String(packetNumber);
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

  Serial.println("MarcoPolo Hider two-way test starting...");
  Serial.println("Hider TX slot: start of each 4 second cycle.");
  Serial.println("Listening between Hider TX slots.");
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
    Serial.println(auxStateText());
    lastStatusTime = now;
  }
}
