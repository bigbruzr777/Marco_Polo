# MarcoPolo Week 1 - Phase 1 LoRa Serial Test

## Project Goal

MarcoPolo is a low-cost Industry 4.0 asset tracking and recovery prototype. The final system will use a Hider tag and a Seeker receiver to help locate tracked assets with LoRa or Bluetooth, and later phases may add GPS, TinyML sensor data, GPS-lost chirping, RSSI-based hotter/colder tracking, visualization, and robot navigation.

## Phase 1 Goal

Phase 1 proves that two EBYTE E32-900T20D LoRa modules can communicate over serial. The current Hider runs on an Arduino Nano 33 BLE Sense, and the Seeker remains on an Arduino Uno R3.

The current test firmware sends Hider GPS status over LoRa every 2 seconds. The Seeker reads its own GPS and prints JSON lines for Node-RED.

The Hider flashes the onboard Nano LED for 5 seconds at startup so it is easy to identify.

The Hider uses a compact plain ASCII packet:

```text
HIDER,12,1,30.421234,-87.216789,8,1.25
```

Fields:

- `HIDER`: packet source
- sequence number
- `1` or `0`: GPS fix flag
- latitude
- longitude
- satellite count
- HDOP

If the Hider has no GPS fix yet, it sends:

```text
HIDER,12,0,0,0,3,0
```

The E32-900T20D UART module did not accept RSSI-related AT commands during testing, so the Seeker currently prints `rssi=NA`.

This phase does not use TinyML, dashboards, or complex packet parsing.

## T-Beam Supreme Hider Test

We also started testing a LilyGO T-Beam Supreme as the next Hider.

Right now this is mostly a screen, GPS, and button test. The T-Beam GPS works, and the small OLED works. The screen is rotated vertical and shows:

```text
Tag 1
GPS Fix / GPS No Fix / GPS OFF
Sat n
```

The screen is a small monochrome SH1106 OLED. It does not do color. It can still show simple text, icons, battery, GPS, LoRa status, arrows, and a tiny breadcrumb-style map later.

T-Beam notes:

- USB serial is on `COM11` right now.
- Board target is currently a generic ESP32-S3 PlatformIO target.
- OLED I2C was found at `0x3D`.
- Display I2C uses SDA `17`, SCL `18`.
- PMU I2C uses SDA `42`, SCL `41`.
- GPS uses RX `9`, TX `8`, and enable pin `7`.
- BOOT button is GPIO `0`.
- BOOT now toggles GPS on/off in firmware.
- RESET stays a normal reset button.
- POWER stays a PMU power button. Hold it about 4 seconds to turn off when running from battery.

The onboard T-Beam SX1262 LoRa radio can send packets with RadioLib, but it did not talk directly to the existing E32/Uno Seeker. That is probably a radio/framing mismatch. For the old Seeker workflow, the easiest path is still using an E32 module on the Hider side or moving the Seeker to an SX1262 receiver too.

## Wiring Summary

Nano 33 BLE Sense Hider wiring:

| Connection | Wiring |
| --- | --- |
| Battery +5V | breadboard + rail |
| Battery GND | breadboard - rail |
| Nano VIN | +5V rail |
| Nano GND | GND rail |
| GPS VCC | Nano 3V3 |
| GPS GND | GND |
| GPS TX | Nano D0/RX |
| GPS RX | Not connected |
| Nano D1/TX | E32 RX |
| E32 TX | Not connected |
| E32 VCC | +5V rail |
| E32 GND | GND rail |
| E32 M0 | GND |
| E32 M1 | GND |
| E32 AUX | Not connected |

The Nano 33 BLE Sense is 3.3V logic only. No resistor divider is needed from Nano D1/TX to E32 RX. The Hider uses `Serial1`: RX D0 receives GPS TX, and TX D1 sends packets to E32 RX.

Physical notes:

- USB points toward the top of the breadboard.
- D13 is at C8.
- VIN is at C22.
- Board bridges the breadboard trench.

Uno Seeker wiring:

| EBYTE E32 Pin | Arduino Uno Pin |
| --- | --- |
| TXD | D10 |
| RXD | D11 through a voltage divider |
| AUX | D4, optional but recommended for diagnostics |
| M0 | GND |
| M1 | GND |
| VCC | Correct module supply voltage |
| GND | GND |

The Seeker code uses `SoftwareSerial loraSerial(10, 11);`.

- Arduino D10 is SoftwareSerial RX and receives from E32 TXD.
- Arduino D11 is SoftwareSerial TX and sends to E32 RXD through a voltage divider.
- Arduino D4 can read E32 AUX. AUX is HIGH when the module is ready and LOW when it is busy.
- The E32 serial baud rate is 9600.

Seeker GPS wiring:

| GPS Pin | Arduino Uno Pin |
| --- | --- |
| VCC | 5V rail |
| GND | GND rail |
| TX | D8 |
| RX | Not connected |

The Seeker uses AltSoftSerial for GPS. On Arduino Uno, AltSoftSerial uses RX on D8 and TX on D9. GPS RX is not used in this phase.

- USB Serial Monitor baud rate is 115200.
- GPS baud rate is 9600.

## Upload the Hider

1. Connect the Hider Nano 33 BLE Sense to USB.
2. In VS Code, open the PlatformIO sidebar.
3. Under `Hider_LoRa`, click `Build`.
4. Click `Upload`.
5. Click `Monitor`.

The Hider PlatformIO target is `nano33ble`. The upload port may change when switching from Uno to Nano.

Command-line option:

```powershell
cd "C:\Users\carre\Documents\School\ISE575\Week 1\MarcoPolo_Wk1\Hider_LoRa"
pio run
pio run --target upload
pio device monitor --baud 115200
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
pio device monitor --baud 115200
```

If both Arduino boards are connected at the same time, PlatformIO may need a specific serial port. List ports with:

```powershell
pio device list
```

Then upload or monitor with the matching port:

```powershell
pio run --target upload --upload-port COM3
pio device monitor --port COM3 --baud 115200
```

Replace `COM3` with the correct port for that board.

## Expected Serial Monitor Output

Hider Serial Monitor:

```text
--- MarcoPolo Hider GPS ---
GPS fix status: FIX
Lat/Lon: 30.421234, -87.216789
GPS sats: 8
GPS hdop: 1.25
Packet sent: HIDER,12,1,30.421234,-87.216789,8,1.25
```

Seeker Serial Monitor prints newline-delimited JSON only:

```json
{"type":"location","name":"Seeker","device":"SEEKER01","source":"gps","fix":true,"lat":30.421000,"lon":-87.216500,"sats":9,"hdop":1.10,"rssi":null,"seq":0,"age_ms":250,"millis":12000}
{"type":"location","name":"Hider","device":"HIDER01","source":"lora","fix":true,"lat":30.421234,"lon":-87.216789,"sats":8,"hdop":1.25,"rssi":null,"seq":12,"age_ms":500,"millis":12001}
```

Unknown lat/lon/rssi values are printed as JSON `null`.

## Node-RED Map and SQLite Logging

The Seeker JSON stream can feed Node-RED for live mapping and SQLite logging.

Install these nodes through Manage Palette:

```text
node-red-node-serialport
node-red-contrib-web-worldmap
node-red-node-sqlite
```

Working flow:

```text
Serial In
  -> JSON
      -> Map Function -> Worldmap
      -> SQLite Function -> Database -> Debug
```

Serial In should read the Seeker USB port at 115200 baud and split on newline. The JSON node must convert the payload from string to Object.

Worldmap URL:

```text
http://localhost:1880/worldmap
```

SQLite database used for the prototype:

```text
C:/Users/carre/Documents/School/ISE575/MarcoPolo/marcopolo.db
```

Table:

```sql
CREATE TABLE IF NOT EXISTS gps_log (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  received_at TEXT DEFAULT CURRENT_TIMESTAMP,
  type TEXT,
  name TEXT,
  device TEXT,
  source TEXT,
  fix INTEGER,
  lat REAL,
  lon REAL,
  sats INTEGER,
  hdop REAL,
  rssi REAL,
  seq INTEGER,
  age_ms INTEGER,
  arduino_millis INTEGER
);
```

SQLite Function uses `msg.topic` for the INSERT. See `LAB_NOTES.md` for the full function code and verification queries.

## Troubleshooting Checklist

- Confirm both E32 modules have matching settings and are on the same channel.
- Confirm both E32 modules are in normal mode: M0 to GND and M1 to GND.
- Confirm Hider GPS TX goes to Nano D0/RX.
- Confirm Hider Nano D1/TX goes to E32 RX.
- Confirm Seeker GPS TX goes to Uno D8.
- Confirm GPS VCC, GND, and antenna placement.
- GPS may take several minutes to get a first fix, especially indoors. `NOFIX` with a satellite count still means GPS serial parsing may be working.
- Wire AUX to Arduino D4 on both boards and check that it reads `HIGH/READY` most of the time.
- If AUX stays `LOW/BUSY`, check module power, M0/M1 mode wiring, and whether the module is stuck starting up.
- If the Hider says `busy pulse seen: NO`, the E32 may not be seeing serial data from Arduino D11.
- Confirm E32 TXD goes to Arduino D10.
- Confirm Arduino D11 goes through a voltage divider to E32 RXD.
- Confirm all grounds are connected together.
- Confirm the Serial Monitor baud rate is 115200.
- Close all Serial Monitor windows before uploading; Windows allows only one program to use a COM port at a time.
- If uploading fails with both boards connected, use `pio device list` and select the correct COM port.
- If the Hider shows `Sent:` messages but the Seeker shows nothing, swap only one variable at a time: wiring, module power, distance between modules, COM port, and E32 channel/settings.
