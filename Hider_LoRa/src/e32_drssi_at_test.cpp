#include <Arduino.h>
#include <SoftwareSerial.h>

// E32 DRSSI / ERSSI AT command experiment.
//
// Current hardware setup for this test:
//   E32 TXD -> Arduino D10
//   Arduino D11 -> voltage divider -> E32 RXD
//   E32 M0  -> 3.3V for configuration / sleep mode
//   E32 M1  -> 3.3V for configuration / sleep mode
//
// USB Serial Monitor: 115200 baud
// E32 UART:           9600 baud
SoftwareSerial e32Serial(10, 11);

const unsigned long RESPONSE_TIMEOUT_MS = 1500;
const unsigned long INTER_COMMAND_DELAY_MS = 500;

const char *commands[] = {
  "AT",
  "AT+HELP=?",
  "AT+DRSSI=?",
  "AT+DRSSI=1",
  "AT+DRSSI=?",
  "AT+ERSSI=?",
  "AT+ERSSI=1",
  "AT+ERSSI=?",
};

struct LineEnding {
  const char *name;
  const char *text;
};

const LineEnding endings[] = {
  {"CRLF", "\r\n"},
  {"CR", "\r"},
  {"LF", "\n"},
  {"NONE", ""},
};

bool printableByte(byte value) {
  return value >= 32 && value <= 126;
}

void printHexByte(byte value) {
  if (value < 0x10) {
    Serial.print('0');
  }

  Serial.print(value, HEX);
}

void drainE32Input() {
  while (e32Serial.available() > 0) {
    e32Serial.read();
  }
}

int readAndPrintResponse() {
  byte responseBytes[320];
  int responseLength = 0;
  unsigned long startMs = millis();

  while (millis() - startMs < RESPONSE_TIMEOUT_MS) {
    while (e32Serial.available() > 0) {
      byte value = e32Serial.read();

      if (responseLength < (int)sizeof(responseBytes)) {
        responseBytes[responseLength] = value;
      }

      responseLength++;
      startMs = millis();
    }
  }

  if (responseLength == 0) {
    Serial.println("Response: NO RESPONSE");
    return 0;
  }

  int storedLength = responseLength;
  if (storedLength > (int)sizeof(responseBytes)) {
    storedLength = sizeof(responseBytes);
  }

  Serial.print("Response: YES, bytes=");
  Serial.println(responseLength);

  Serial.print("ASCII: ");
  for (int i = 0; i < storedLength; i++) {
    byte value = responseBytes[i];

    if (printableByte(value)) {
      Serial.write(value);
    } else if (value == '\r') {
      Serial.print("<CR>");
    } else if (value == '\n') {
      Serial.print("<LF>");
    } else {
      Serial.print('.');
    }
  }

  if (responseLength > storedLength) {
    Serial.print(" ...TRUNCATED");
  }

  Serial.println();

  Serial.print("HEX: ");
  for (int i = 0; i < storedLength; i++) {
    printHexByte(responseBytes[i]);

    if (i < storedLength - 1) {
      Serial.print(' ');
    }
  }

  if (responseLength > storedLength) {
    Serial.print(" ...TRUNCATED");
  }

  Serial.println();
  return responseLength;
}

bool tryCommandWithEnding(const char *command, LineEnding ending) {
  drainE32Input();

  Serial.print("Sending command: ");
  Serial.print(command);
  Serial.print(" ending=");
  Serial.println(ending.name);

  e32Serial.print(command);
  e32Serial.print(ending.text);
  e32Serial.flush();

  int responseLength = readAndPrintResponse();
  Serial.println(responseLength > 0 ? "Attempt result: RESPONSE" : "Attempt result: NO RESPONSE");
  Serial.println();

  return responseLength > 0;
}

void runCommandTest(const char *command) {
  Serial.println("--------------------------------------------------");
  Serial.print("Command under test: ");
  Serial.println(command);

  bool commandProducedResponse = false;

  for (unsigned int i = 0; i < sizeof(endings) / sizeof(endings[0]); i++) {
    commandProducedResponse = tryCommandWithEnding(command, endings[i]);

    if (commandProducedResponse) {
      break;
    }

    delay(INTER_COMMAND_DELAY_MS);
  }

  Serial.print("COMMAND SUMMARY ");
  Serial.print(command);
  Serial.print(": ");
  Serial.println(commandProducedResponse ? "PRODUCED RESPONSE" : "NO RESPONSE WITH ANY LINE ENDING");
}

void setup() {
  Serial.begin(115200);
  e32Serial.begin(9600);

  delay(2000);

  Serial.println();
  Serial.println("E32 DRSSI / ERSSI AT command test");
  Serial.println("Verify hardware before reading results:");
  Serial.println("- E32 M0 must be HIGH / 3.3V.");
  Serial.println("- E32 M1 must be HIGH / 3.3V.");
  Serial.println("- E32 TXD -> Arduino D10.");
  Serial.println("- Arduino D11 -> voltage divider -> E32 RXD.");
  Serial.println("- USB Serial Monitor must be 115200 baud.");
  Serial.println("- E32 UART is tested at 9600 baud.");
  Serial.println();
  Serial.println("This is an experiment. Do not assume RSSI works unless");
  Serial.println("AT+DRSSI=1 and AT+DRSSI=? produce valid responses.");
  Serial.println();

  for (unsigned int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    runCommandTest(commands[i]);
    delay(INTER_COMMAND_DELAY_MS);
  }

  Serial.println("--------------------------------------------------");
  Serial.println("AT command test complete.");
  Serial.println("If DRSSI was enabled successfully, return M0/M1 to GND");
  Serial.println("and check whether received data has an extra RSSI byte.");
}

void loop() {
  // Test runs once in setup().
}
