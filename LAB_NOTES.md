# MarcoPolo Lab Notes

## Current State

We have a working T-Beam prototype.

- Hider: LilyGO T-Beam Supreme, usually `COM11`
- Seeker: regular LilyGO T-Beam SX1276, usually `COM12`
- Hider GPS works
- Seeker GPS works
- Hider sends GPS over LoRa
- Seeker reads LoRa RSSI
- Hider advertises BLE as `MP-HIDER01`
- Seeker uses BLE RSSI when nearby
- Hider sends a movement alert from its QMI8658 IMU
- Seeker publishes GPS and LoRa telemetry to the NUC over MQTT
- Seeker uses a directional 915 MHz antenna for sweep testing

Current data path:

```text
Hider GPS -> LoRa -> Seeker -> Wi-Fi/MQTT -> NUC Node-RED
Hider BLE beacon -> Seeker close-range RSSI
Hider IMU movement -> LoRa ALERT -> Seeker MOTION ALERT line
```

## Hider

Normal LoRa packet:

```text
HIDER,<seq>,<fix>,<lat>,<lon>,<sats>,<hdop>
```

Moving packet:

```text
HIDER,<seq>,<fix>,<lat>,<lon>,<sats>,<hdop>,ALERT
```

Optional tagged fields are appended without changing the original packet:

```text
M=0/1
BV=<battery volts>
BP=<battery percent>
UP=<Hider uptime ms>
```

Live example:

```text
HIDER,47,1,30.411110,-86.715064,12,0.68,M=0,BV=4.109,BP=93,UP=85713
```

No GPS fix:

```text
HIDER,<seq>,0,0,0,<sats>,99.99
```

Hider screen:

```text
Tag 1
GPS Fix / GPS No Fix / GPS OFF
Sat n
```

BOOT toggles GPS on/off. RESET resets. POWER is the PMU power button.

## Movement Alert

The Hider uses the QMI8658 IMU. When movement starts, it sends an alert immediately and then sends alert packets once per second. It keeps the alert active for about eight seconds after the last detected movement.

Seeker warning timing:

```text
Last alert packet was less than 5 seconds ago: fast flash
Last alert packet was 5-20 seconds ago: solid warning
No alert for 20 seconds: warning removed
```

Serial proof on the Hider:

```text
Motion: ALERT
Packet: HIDER,...,ALERT
```

The Seeker shows:

```text
MOTION ALERT!
```

The alert flashes on the bottom line without replacing the normal GPS, LoRa, or BLE lock label.

The first motion tests failed because the IMU and LoRa were both assigned to `FSPI`. LoRa setup remapped that SPI peripheral, so the IMU returned zeros even though initialization reported success. The fix was:

```text
LoRa: FSPI
QMI8658 IMU: HSPI
```

After separating the buses, moving the Hider produced real acceleration/gyro values and immediate `HIDER,...,ALERT` packets.

## Seeker

The Seeker prints JSON only during normal operation.

Example:

```json
{"type":"location","name":"Hider","device":"HIDER01","source":"lora","fix":true,"lat":30.411095,"lon":-86.715012,"sats":12,"hdop":0.95,"rssi":-22.0,"seq":621,"age_ms":1558,"millis":10022}
```

Main screen:

```text
GPS Lock / LoRa Lock / BLE Lock / No Lock
<10 FT / <20 FT / ...
NORTH / NORTHEAST / EAST / ...
or FROZEN ... ON FIRE!
MOTION ALERT!
```

The display is landscape. When both GPS fixes are valid, it shows the compass bearing from Seeker to Hider. If either fix is missing, it falls back to the heat scale. A heart flashes when a Hider packet arrives.

Bottom button `IO38` switches to screen 2:

```text
GPS fix: north-up radar with Seeker in the center and Hider as the target
No GPS fix: large signal-strength bars for antenna sweeping
```

The GPS radar is north-up because the regular T-Beam does not provide a reliable stationary compass heading. It shows the correct map bearing without pretending to know which way the user is facing.

## Directional Antenna

No LoRa frequency or packet settings changed. The directional antenna changes how RSSI should be used:

- Keep the antenna upright and use the same polarization for each reading.
- Point, pause for several packet heartbeats, then compare.
- Do not react to one brief RSSI jump.
- Repeat a sweep because reflections can create false peaks indoors.

The first live test after this change still received valid Hider GPS packets. RSSI was about `-57` to `-59 dBm` in the tested direction.

## BLE Handoff

BLE no longer switches on at one threshold crossing.

- Enter BLE after three readings at `-67 dBm` or stronger.
- Leave BLE after three readings at `-78 dBm` or weaker.
- BLE `<10 FT`: `-55 dBm` or stronger.
- BLE `<25 FT`: `-65 dBm` or stronger.
- Remaining BLE lock: `<50 FT`.

These are starting estimates for indoor use. Walls, body position, antenna orientation, and reflections will affect them.

## Charging Check

The Hider PMU reported:

```text
Battery: 79% 4004mV charging=yes
```

The Hider battery icon draws a small lightning bolt while charging or powered from USB.

## Heat Scale

BLE is treated as the close-range handoff, so once BLE is selected the display starts at WARM.

- ON FIRE!
- VERY HOT
- HOT
- WARM
- LUKE WARM
- COOL
- COLD
- VERY COLD
- FROZEN
- UNK

These labels are still rough. Walking tests should tune them.

## Node-RED

Installed nodes:

```text
node-red-node-serialport
node-red-contrib-web-worldmap
node-red-node-sqlite
```

Working flow:

```text
Seeker -> MarcoPolo Wi-Fi -> MQTT -> Node-RED
```

MQTT broker:

```text
10.42.0.1:1883
marcopolo/seeker/telemetry
```

The Seeker still prints USB serial JSON at `115200` for bench checks.

The MQTT message keeps the original map fields and adds optional values for:

```text
source, mode, ble_rssi
gps_satellites, gps_hdop, gps_speed_mps, gps_course_deg
hider_motion, motion_alert
battery_v, battery_pct
hider_battery_v, hider_battery_pct
hider_uptime_ms, packet_seq, packet_age_ms, uptime_ms
```

PubSubClient uses a `1024` byte buffer for the expanded JSON. Successful publish logging is throttled to once every 10 seconds.

Useful checks:

```sql
SELECT COUNT(*) AS total_rows FROM gps_log;
```

```sql
SELECT received_at, name, source, fix, lat, lon, sats, hdop, rssi, seq, age_ms
FROM gps_log
ORDER BY id DESC
LIMIT 20;
```

## Build Commands

Seeker:

```powershell
cd "C:\Users\carre\Documents\School\ISE575\Week 1\MarcoPolo_Wk1\Seeker_LoRa"
C:\Users\carre\.platformio\penv\Scripts\platformio.exe run
C:\Users\carre\.platformio\penv\Scripts\platformio.exe run --target upload
```

Hider:

```powershell
cd "C:\Users\carre\Documents\School\ISE575\MarcoPolo\Hider_LoRa"
C:\Users\carre\.platformio\penv\Scripts\platformio.exe run
C:\Users\carre\.platformio\penv\Scripts\platformio.exe run --target upload
```

Close Node-RED and all serial monitors before uploading.

## Pin Notes

T-Beam Supreme Hider:

- OLED SDA `17`, SCL `18`
- PMU SDA `42`, SCL `41`
- GPS RX `9`, TX `8`
- GPS enable `7`
- BOOT `0`
- LoRa CS `10`, DIO1 `1`, RST `5`, BUSY `4`
- LoRa SCK `12`, MISO `13`, MOSI `11`
- QMI8658 IMU CS `34`, MOSI `35`, SCK `36`, MISO `37`

Regular T-Beam Seeker:

- GPS RX `34`, TX `12`
- OLED/PMU SDA `21`, SCL `22`
- OLED reset `16`
- LoRa NSS `18`
- LoRa SCK `5`, MISO `19`, MOSI `27`
- LoRa RST `23`, DIO0 `26`, DIO1 `33`
- User button `38`

## E32 Notes

The project started with EBYTE E32-900T20D modules on Arduino boards. That proved the first LoRa link.

Important voltage-divider fix:

```text
Arduino D11 -> 1k -> E32 RX
E32 RX -> 2k -> GND
```

That gives about `3.33V`. The reversed divider only made about `1.67V`, which was too low.

We also tested:

```text
AT+DRSSI=?
AT+DRSSI=1
AT+ERSSI=?
AT+ERSSI=1
```

Our E32 modules returned `ERR`, so we moved to T-Beam radios where RadioLib gives packet RSSI.

## Next Tests

- Sweep the directional antenna in fixed steps and record stable RSSI readings.
- Check the antenna front/back response outdoors before indoor tests.
- Move close and confirm BLE handoff feels reasonable.
- Tune heat labels.
- Tune movement sensitivity if needed.
- Confirm SQLite keeps logging Hider and Seeker rows.
- Decide later whether compass/IMU heading is needed on the Seeker.

## Sources

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
- E32-900T20D manual: https://www.manualslib.com/manual/3752089/Ebyte-E32-900t20d.html
- E32 V8.2 manual: https://robu.in/wp-content/uploads/2024/07/E32-xxxT20x-V8.2_UserManual_EN_V1.0.pdf
