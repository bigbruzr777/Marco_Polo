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

unsigned long packetNumber = 1;

void setup() {
  // USB Serial Monitor for messages to the computer.
  Serial.begin(9600);

  // Serial link between the Arduino and the E32 LoRa module.
  loraSerial.begin(9600);

  Serial.println("MarcoPolo Hider starting...");
  Serial.println("Sending one test packet every 2 seconds.");
}

void loop() {
  // Build a simple text packet. No GPS, TinyML, or parsing yet.
  String packet = "HIDER01,TEST,PKT=" + String(packetNumber);

  // Send the packet over LoRa.
  loraSerial.println(packet);

  // Also print the packet to the USB Serial Monitor.
  Serial.print("Sent: ");
  Serial.println(packet);

  packetNumber++;

  // Wait 2 seconds before sending the next packet.
  delay(2000);
}
