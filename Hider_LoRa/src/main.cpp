#include <Arduino.h>
#include <SoftwareSerial.h>

// MarcoPolo Phase 1 - Hider / transmitter
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

unsigned long packetNumber = 1;

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

  // Serial link between the Arduino and the E32 LoRa module.
  loraSerial.begin(9600);

  Serial.println("MarcoPolo Hider starting...");
  Serial.println("Sending one test packet every 2 seconds.");
  Serial.print("AUX pin D4 state: ");
  Serial.println(auxStateText());
}

void loop() {
  // Build a simple text packet. No GPS, TinyML, or parsing yet.
  String packet = "HIDER01,TEST,PKT=" + String(packetNumber);

  bool auxWasBusy = false;

  // Send the packet over LoRa.
  loraSerial.println(packet);

  // Watch AUX briefly after sending. A LOW pulse means the E32 accepted work.
  unsigned long watchStart = millis();
  while (millis() - watchStart < 200) {
    if (digitalRead(LORA_AUX_PIN) == LOW) {
      auxWasBusy = true;
    }
  }

  // Also print the packet to the USB Serial Monitor.
  Serial.print("Sent: ");
  Serial.println(packet);
  Serial.print("AUX after send: ");
  Serial.print(auxStateText());
  Serial.print(" busy pulse seen: ");
  Serial.println(auxWasBusy ? "YES" : "NO");

  packetNumber++;

  // Wait 2 seconds before sending the next packet.
  delay(2000);
}
