# MarcoPolo Week 1 - Phase 1 LoRa Serial Test

## Project Goal

MarcoPolo is a low-cost Industry 4.0 asset tracking and recovery prototype. The final system will use a Hider tag and a Seeker receiver to help locate tracked assets with LoRa or Bluetooth, and later phases may add GPS, TinyML sensor data, GPS-lost chirping, RSSI-based hotter/colder tracking, visualization, and robot navigation.

## Phase 1 Goal

Phase 1 only proves that two EBYTE E32-900T20D LoRa modules can communicate through two Arduino Uno R3 boards over serial.

The current test firmware is a two-way heartbeat test. The Hider sends near the start of each 4 second cycle, and the Seeker sends about 2 seconds later. Both boards listen between their transmit slots.

This phase does not use GPS, TinyML, dashboards, RSSI tracking, or complex packet parsing.

## Wiring Summary

Use this wiring on both Arduino Uno boards:

| EBYTE E32 Pin | Arduino Uno Pin |
| --- | --- |
| TXD | D10 |
| RXD | D11 through a voltage divider |
| AUX | D4, optional but recommended for diagnostics |
| M0 | GND |
| M1 | GND |
| VCC | Correct module supply voltage |
| GND | GND |

The code uses `SoftwareSerial loraSerial(10, 11);`.

- Arduino D10 is SoftwareSerial RX and receives from E32 TXD.
- Arduino D11 is SoftwareSerial TX and sends to E32 RXD through a voltage divider.
- Arduino D4 can read E32 AUX. AUX is HIGH when the module is ready and LOW when it is busy.
- The serial baud rate is 9600.

## Upload the Hider

1. Connect the Hider Arduino Uno to USB.
2. In VS Code, open the PlatformIO sidebar.
3. Under `Hider_LoRa`, click `Build`.
4. Click `Upload`.
5. Click `Monitor`.

This project is configured for the Hider on `COM7`.

Command-line option:

```powershell
cd "C:\Users\carre\Documents\School\ISE575\Week 1\MarcoPolo_Wk1\Hider_LoRa"
pio run
pio run --target upload
pio device monitor --baud 9600
```

## Upload the Seeker

1. Connect the Seeker Arduino Uno to USB.
2. In VS Code, open the PlatformIO sidebar.
3. Under `Seeker_LoRa`, click `Build`.
4. Click `Upload`.
5. Click `Monitor`.

This project is configured for the Seeker on `COM8`.

Command-line option:

```powershell
cd "C:\Users\carre\Documents\School\ISE575\Week 1\MarcoPolo_Wk1\Seeker_LoRa"
pio run
pio run --target upload
pio device monitor --baud 9600
```

If both Arduino boards are connected at the same time, PlatformIO may need a specific serial port. List ports with:

```powershell
pio device list
```

Then upload or monitor with the matching port:

```powershell
pio run --target upload --upload-port COM3
pio device monitor --port COM3 --baud 9600
```

Replace `COM3` with the correct port for that board.

## Expected Serial Monitor Output

Hider Serial Monitor:

```text
TX: HIDER01,HEARTBEAT,PKT=1
RX: SEEKER01,HEARTBEAT,PKT=1
TX: HIDER01,HEARTBEAT,PKT=2
RX: SEEKER01,HEARTBEAT,PKT=2
```

Seeker Serial Monitor:

```text
RX: HIDER01,HEARTBEAT,PKT=1
TX: SEEKER01,HEARTBEAT,PKT=1
RX: HIDER01,HEARTBEAT,PKT=2
TX: SEEKER01,HEARTBEAT,PKT=2
```

With AUX wired to D4, the monitors also print AUX diagnostics:

```text
AUX pin D4 state: HIGH/READY
Waiting... AUX=HIGH/READY
```

## Troubleshooting Checklist

- Confirm both E32 modules have matching settings and are on the same channel.
- Confirm both E32 modules are in normal mode: M0 to GND and M1 to GND.
- Wire AUX to Arduino D4 on both boards and check that it reads `HIGH/READY` most of the time.
- If AUX stays `LOW/BUSY`, check module power, M0/M1 mode wiring, and whether the module is stuck starting up.
- If the Hider says `busy pulse seen: NO`, the E32 may not be seeing serial data from Arduino D11.
- Confirm E32 TXD goes to Arduino D10.
- Confirm Arduino D11 goes through a voltage divider to E32 RXD.
- Confirm all grounds are connected together.
- Confirm the Serial Monitor baud rate is 9600.
- Close all Serial Monitor windows before uploading; Windows allows only one program to use a COM port at a time.
- If uploading fails with both boards connected, use `pio device list` and select the correct COM port.
- If the Hider shows `Sent:` messages but the Seeker shows nothing, swap only one variable at a time: wiring, module power, distance between modules, COM port, and E32 channel/settings.
