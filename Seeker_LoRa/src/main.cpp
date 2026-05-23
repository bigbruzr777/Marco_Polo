#include <Arduino.h>
#include <SoftwareSerial.h>

// MarcoPolo Phase 1 - Seeker / receiver
//
// Wiring for the EBYTE E32 module:
//   E32 TXD -> Arduino D10
//   Arduino D11 -> voltage divider -> E32 RXD
//   E32 M0  -> GND
//   E32 M1  -> GND
//
// SoftwareSerial pin order is: RX pin, TX pin.
// D10 receives data from the LoRa module.
// D11 sends data to the LoRa module.
SoftwareSerial loraSerial(10, 11);

// Optional diagnostic wiring:
//   E32 AUX -> Arduino D4
//
// AUX is HIGH when the E32 module is ready.
// AUX is LOW when the E32 module is busy sending, receiving, or starting up.
const int LORA_AUX_PIN = 4;

String incomingPacket = "";
unsigned long lastStatusTime = 0;
int lastAuxState = HIGH;

String auxStateText() {
  if (digitalRead(LORA_AUX_PIN) == HIGH) {
    return "HIGH/READY";
  }

  return "LOW/BUSY";
}

void setup() {
  // USB Serial Monitor for messages to the computer.
  Serial.begin(9600);

  // INPUT_PULLUP keeps D4 from floating if AUX is not connected yet.
  pinMode(LORA_AUX_PIN, INPUT_PULLUP);
  lastAuxState = digitalRead(LORA_AUX_PIN);

  // Serial link between the Arduino and the E32 LoRa module.
  loraSerial.begin(9600);

  Serial.println("MarcoPolo Seeker starting...");
  Serial.println("Waiting for LoRa packets...");
  Serial.print("AUX pin D4 state: ");
  Serial.println(auxStateText());
}

void loop() {
  int auxState = digitalRead(LORA_AUX_PIN);
  if (auxState != lastAuxState) {
    Serial.print("AUX changed: ");
    Serial.println(auxStateText());
    lastAuxState = auxState;
  }

  // Print a slow heartbeat so the Serial Monitor shows the sketch is running.
  if (millis() - lastStatusTime >= 5000) {
    Serial.print("Waiting... AUX=");
    Serial.println(auxStateText());
    lastStatusTime = millis();
  }

  // Read every available byte from the E32 module.
  while (loraSerial.available() > 0) {
    char receivedChar = loraSerial.read();

    // The Hider sends packets with println(), so each packet ends with '\n'.
    if (receivedChar == '\n') {
      String packet = incomingPacket;
      packet.trim();
      incomingPacket = "";

      if (packet.length() > 0) {
        Serial.print("Received: ");
        Serial.println(packet);
      }
    } else {
      // Store the byte until the full line arrives.
      incomingPacket += receivedChar;
    }

    // Keep the packet buffer from growing forever if noise or partial data arrives.
    if (incomingPacket.length() > 80) {
      Serial.print("Partial/overflow packet cleared: ");
      Serial.println(incomingPacket);
      incomingPacket = "";
    }
  }
}
