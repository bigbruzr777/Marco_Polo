# MarcoPolo Lab Notes

## Current Status

- Two Arduino Uno boards talk through EBYTE E32-900T20D modules.
- Hider reads GPS on D8 and sends a LoRa line every 2 seconds.
- Seeker reads its GPS on D8, receives Hider packets, and prints both positions.
- USB Serial is 115200. E32 and GPS are 9600.

Hider packet:

```text
HIDER,<fix>,<lat>,<lon>,<sats>,<hdop>
```

Examples:

```text
HIDER,1,30.421234,-87.216789,8,1.25
HIDER,0,0,0,3,0
```

## Wiring Notes

E32:

- M0 = GND
- M1 = GND
- E32 TXD -> Arduino D10
- Arduino D11 -> divider -> E32 RXD
- AUX -> Arduino D4

GPS:

- GPS VCC -> 5V rail
- GPS GND -> GND rail
- GPS TX -> Arduino D8
- GPS RX not connected

## Voltage Divider Fix

The first E32 RX divider was backwards:

```text
D11 -> 2k -> E32 RX
E32 RX -> 1k -> GND
```

That made about 1.67V:

```text
5V x (1k / (2k + 1k)) = 1.67V
```

Correct divider:

```text
D11 -> 1k -> E32 RX
E32 RX -> 2k -> GND
```

That makes about 3.33V:

```text
5V x (2k / (1k + 2k)) = 3.33V
```

Breadboard fix:

- D11 jumper to F20
- 1k from G20 to G26
- 2k from H26 to right - rail
- E32 RX at J26

After the fix, AUX showed transmit activity and the Seeker received packets.

## RSSI Notes

The current E32-900T20D modules do not give us usable RSSI in this setup.

We tested newer documented commands with M0/M1 HIGH:

```text
AT+DRSSI=?
AT+DRSSI=1
AT+ERSSI=?
AT+ERSSI=1
```

Both modules returned ERR. The Seeker module's `AT+HELP=?` response did not list DRSSI or ERSSI.

Current firmware prints:

```text
rssi=NA
```

For hotter/colder later, use packet age, missed packets, GPS distance, or switch to an SPI LoRa radio/library that exposes packet RSSI.

## Sources

Low-effort links used or useful for this project:

- PlatformIO Arduino Uno board setup: https://docs.platformio.org/en/latest/boards/atmelavr/uno.html
- Arduino SoftwareSerial: https://docs.arduino.cc/tutorials/communication/SoftwareSerialExample
- TinyGPSPlus GPS parser: https://github.com/mikalhart/TinyGPSPlus
- AltSoftSerial GPS serial on Uno D8/D9: https://github.com/PaulStoffregen/AltSoftSerial
- E32-900T20D manual, pins, AUX, modes: https://www.manualslib.com/manual/3752089/Ebyte-E32-900t20d.html
- E32-900T20D PDF mirror: https://e-gizmo.net/oc/kits%20documents/Wireless%20Modules/E32-900T20D.pdf
- E32 V8.2 manual with DRSSI/ERSSI commands: https://robu.in/wp-content/uploads/2024/07/E32-xxxT20x-V8.2_UserManual_EN_V1.0.pdf
- Voltage divider math: https://polluxlabs.io/knowledge/electronics-basics/understanding-voltage-dividers
- 5V to 3.3V divider example: https://zbotic.in/logic-level-shifter-5v-to-3-3v-and-back-conversion-guide/
- Future RSSI option, RadioLib SX1276 `getRSSI()`: https://jgromes.github.io/RadioLib/class_s_x1276.html
