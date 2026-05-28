# MarcoPolo Lab Notes

## Current Status

- Two Arduino Uno boards talk through EBYTE E32-900T20D modules.
- Hider reads GPS on D8 and sends a LoRa line every 2 seconds.
- Hider flashes the onboard LED for 5 seconds at startup.
- Seeker reads its GPS on D8, receives Hider packets, and prints JSON lines for Node-RED.
- USB Serial is 115200. E32 and GPS are 9600.

Hider packet:

```text
HIDER,<seq>,<fix>,<lat>,<lon>,<sats>,<hdop>
```

Examples:

```text
HIDER,12,1,30.421234,-87.216789,8,1.25
HIDER,12,0,0,0,3,0
```

Seeker output is newline-delimited JSON only:

```json
{"type":"location","name":"Hider","device":"HIDER01","source":"lora","fix":true,"lat":30.421234,"lon":-87.216789,"sats":8,"hdop":1.25,"rssi":null,"seq":12,"age_ms":500,"millis":12001}
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

## Node-RED Mapping and SQLite Logging

The Seeker now prints newline-delimited JSON over USB Serial. Node-RED reads that stream, plots live markers, and logs rows into SQLite.

Example Seeker JSON:

```json
{"type":"location","name":"Hider","device":"HIDER01","source":"lora","fix":true,"lat":30.411094,"lon":-86.715057,"sats":9,"hdop":1.05,"rssi":null,"seq":1349,"age_ms":1702,"millis":40026}
{"type":"location","name":"Seeker","device":"SEEKER01","source":"gps","fix":true,"lat":30.411111,"lon":-86.715049,"sats":10,"hdop":0.81,"rssi":null,"seq":0,"age_ms":184,"millis":42009}
```

Installed Node-RED nodes:

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

Serial In reads the Seeker USB serial port at 115200 baud and splits strings by newline. The JSON node converts each line from a string into an object.

Worldmap needs at least:

```text
name
lat
lon
```

The map is at:

```text
http://localhost:1880/worldmap
```

SQLite database:

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

Working SQLite Function:

```js
let p = msg.payload;

if (typeof p === "string") {
    try {
        p = JSON.parse(p);
    } catch (err) {
        node.warn("Bad JSON: " + msg.payload);
        return null;
    }
}

if (!p || p.type !== "location") {
    return null;
}

function sqlString(value) {
    if (value === null || value === undefined) return "NULL";
    return "'" + String(value).replace(/'/g, "''") + "'";
}

function sqlNumber(value) {
    if (value === null || value === undefined || value === "") return "NULL";
    return Number(value);
}

msg.topic = `
INSERT INTO gps_log (
  type,
  name,
  device,
  source,
  fix,
  lat,
  lon,
  sats,
  hdop,
  rssi,
  seq,
  age_ms,
  arduino_millis
) VALUES (
  ${sqlString(p.type)},
  ${sqlString(p.name)},
  ${sqlString(p.device)},
  ${sqlString(p.source)},
  ${p.fix ? 1 : 0},
  ${sqlNumber(p.lat)},
  ${sqlNumber(p.lon)},
  ${sqlNumber(p.sats)},
  ${sqlNumber(p.hdop)},
  ${sqlNumber(p.rssi)},
  ${sqlNumber(p.seq)},
  ${sqlNumber(p.age_ms)},
  ${sqlNumber(p.millis)}
);
`;

return msg;
```

Database node:

```text
Database: C:/Users/carre/Documents/School/ISE575/MarcoPolo/marcopolo.db
SQL source: msg.topic
```

Useful checks:

```sql
SELECT COUNT(*) AS total_rows FROM gps_log;
```

```sql
SELECT *
FROM gps_log
ORDER BY id DESC
LIMIT 20;
```

```sql
SELECT received_at, name, device, source, fix, lat, lon, sats, hdop, seq, age_ms
FROM gps_log
ORDER BY id DESC
LIMIT 20;
```

Notes:

- Arduino serial must be JSON only.
- Each object is one `Serial.println()` line.
- Serial In splits on newline.
- The JSON node must output an Object before map/database branches.
- `array[0] [empty]` after an INSERT is normal.
- Rows with all NULL values mean the INSERT ran but values were not mapped correctly.
- Direct SQL string generation is fine for this low-rate prototype.
- Disable extra Debug nodes after testing.

Completed data path:

```text
Hider GPS -> E32 LoRa -> Seeker -> USB Serial JSON -> Node-RED -> Worldmap + SQLite -> DB Browser
```

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
- Node-RED Serial node: https://flows.nodered.org/node/node-red-node-serialport
- Node-RED Worldmap node: https://flows.nodered.org/node/node-red-contrib-web-worldmap
- Node-RED SQLite node: https://flows.nodered.org/node/node-red-node-sqlite
