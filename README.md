# MarcoPolo

MarcoPolo is a small ISE 575 asset recovery prototype. The current version uses a T-Beam Supreme as the tag and a regular T-Beam as the handheld seeker.

## Current Setup

- Hider: LilyGO T-Beam Supreme, usually `COM11`
- Seeker: LilyGO T-Beam SX1276, usually `COM12`
- Hider sends GPS packets over LoRa
- Hider advertises BLE as `MP-HIDER01`
- Hider sends an `ALERT` when it is moving
- Seeker reads its own GPS, receives LoRa, scans BLE, and prints JSON for Node-RED

The recovery flow is:

```text
GPS when both devices have a fix
LoRa RSSI when GPS is missing or range is longer
BLE RSSI when the seeker is close to the hider
```

## LoRa Packet

Normal packet:

```text
HIDER,<seq>,<fix>,<lat>,<lon>,<sats>,<hdop>
```

Example:

```text
HIDER,42,1,30.411094,-86.715057,9,1.05
```

Moving alert packet:

```text
HIDER,43,1,30.411094,-86.715057,9,1.05,ALERT
```

The Hider may append tagged telemetry after the original fields:

```text
HIDER,43,1,30.411094,-86.715057,9,1.05,ALERT,M=1,BV=4.109,BP=93,UP=85713
```

`M` is motion, `BV` is battery volts, `BP` is battery percent, and `UP` is Hider uptime in milliseconds. Older Seeker code can still read the unchanged first seven fields.

If the Hider GPS is not fixed, it still sends:

```text
HIDER,44,0,0,0,3,99.99
```

## Seeker Output

The Seeker prints newline-delimited JSON at `115200` baud for Node-RED.

```json
{"type":"location","name":"Hider","device":"HIDER01","source":"lora","fix":true,"lat":30.411095,"lon":-86.715012,"sats":12,"hdop":0.95,"rssi":-22.0,"seq":621,"age_ms":1558,"millis":10022}
{"type":"location","name":"Seeker","device":"SEEKER01","source":"gps","fix":true,"lat":30.411134,"lon":-86.715018,"sats":8,"hdop":1.04,"rssi":null,"seq":0,"age_ms":706,"millis":10011}
```

Node-RED can map these directly because valid fixes include `name`, `lat`, and `lon`.

## Screens

Hider screen:

```text
Tag 1
GPS Fix / GPS No Fix / GPS OFF
Sat n
```

Seeker main screen:

```text
GPS Lock / LoRa Lock / BLE Lock / No Lock
<10 FT / <20 FT / ...
NORTH / NORTHEAST / EAST / ...
or FROZEN ... ON FIRE!
MOTION ALERT!
```

When both GPS fixes are valid, the main screen shows the cardinal bearing from Seeker to Hider. If either GPS fix is missing, it shows the heat scale instead. The small heart flashes for every Hider packet.

The Seeker bottom button, `IO38`, switches to screen 2:

```text
GPS fix: north-up target radar
No GPS fix: large signal-strength bars
```

The radar shows the target relative to north. It does not claim to know which way the Seeker is physically facing.

BLE only takes over after three readings of at least `-67 dBm`. It switches back after three readings at or below `-78 dBm`, so the display does not bounce between LoRa and BLE. BLE distance bands are rough indoor estimates:

```text
-55 dBm or stronger: <10 FT
-65 dBm or stronger: <25 FT
weaker BLE lock: <50 FT
```

## Movement Alert

The Hider uses the T-Beam Supreme QMI8658 IMU. When movement starts, it immediately sends an alert packet. It keeps sending alert packets every second until it has been still for about eight seconds. After the latest alert packet, the Seeker flashes `MOTION ALERT!` quickly for five seconds, holds it steadily for another fifteen seconds, then removes it.

When USB power is connected and the PMU reports charging, the Hider battery icon shows a small lightning bolt.

Useful Hider serial lines:

```text
Motion: ALERT
Packet: HIDER,...,ALERT
```

## Build And Upload

Close Node-RED, PlatformIO Monitor, and any serial terminal before uploading.

Seeker:

```powershell
cd "C:\Users\carre\Documents\School\ISE575\Week 1\MarcoPolo_Wk1\Seeker_LoRa"
C:\Users\carre\.platformio\penv\Scripts\platformio.exe run
C:\Users\carre\.platformio\penv\Scripts\platformio.exe run --target upload
C:\Users\carre\.platformio\penv\Scripts\platformio.exe device monitor --port COM12 --baud 115200
```

Hider:

```powershell
cd "C:\Users\carre\Documents\School\ISE575\MarcoPolo\Hider_LoRa"
C:\Users\carre\.platformio\penv\Scripts\platformio.exe run
C:\Users\carre\.platformio\penv\Scripts\platformio.exe run --target upload
C:\Users\carre\.platformio\penv\Scripts\platformio.exe device monitor --port COM11 --baud 115200
```

## Node-RED

Current NUC flow:

```text
Seeker -> Wi-Fi -> MQTT -> Node-RED -> map + logging
```

- Broker: `10.42.0.1:1883`
- Topic: `marcopolo/seeker/telemetry`
- The USB JSON output is still available for bench testing.

The original MQTT fields remain unchanged:

```text
device, hider_lat, hider_lon, seeker_lat, seeker_lon, rssi, snr
```

Optional fields now include GPS quality, BLE RSSI, motion, both battery readings, packet sequence, packet age, and device uptimes. Node-RED can leave unavailable CSV values blank.

## Troubleshooting

- Upload fails: close Node-RED and serial monitors.
- No Hider lock: check Hider power, antenna, and LoRa packets on COM11.
- No Seeker GPS: move near a window or outside.
- No BLE handoff: confirm Hider was uploaded with BLE beacon firmware.
- Movement alert too sensitive or too quiet: tune `MOTION_ACCEL_DELTA`, `MOTION_GYRO_DPS`, and `MOTION_STILL_MS` in the Hider firmware.

## Useful Links

- LilyGO LoRa examples: https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series
- LilyGO T-Beam: https://lilygo.cc/en-us/products/t-beam
- T-Beam Supreme docs: https://wiki.lilygo.cc/products/t-beam-series/t-beam-supreme/
- SensorLib: https://github.com/lewisxhe/SensorLib
- TinyGPSPlus: https://github.com/mikalhart/TinyGPSPlus
- RadioLib SX1276: https://jgromes.github.io/RadioLib/class_s_x1276.html
- RadioLib SX1262: https://jgromes.github.io/RadioLib/class_s_x1262.html
- U8g2: https://github.com/olikraus/u8g2
- U8g2 display setup and rotation: https://github.com/olikraus/u8g2/wiki/u8g2setupcpp
- XPowersLib: https://github.com/lewisxhe/XPowersLib
- ESP32 BLE Arduino: https://github.com/espressif/arduino-esp32/tree/master/libraries/BLE
- Node-RED Serial: https://flows.nodered.org/node/node-red-node-serialport
- Node-RED Worldmap: https://flows.nodered.org/node/node-red-contrib-web-worldmap
- Node-RED SQLite: https://flows.nodered.org/node/node-red-node-sqlite
