# MarcoPolo Lab Notes

## Phase 1 Status

Phase 1 communication between the two Arduino Uno R3 boards and EBYTE E32-900T20D LoRa modules is working.

The current firmware runs a two-way heartbeat test:

- Hider transmits `MP1,HIDER01,BEACON,...`
- Seeker transmits `MP1,SEEKER01,STATUS,...`
- Both boards listen between transmit slots.
- AUX diagnostics show `busy pulse seen: YES` when the E32 accepts a UART transmit packet.

## RSSI Notes

The EBYTE E32-900T20D is a UART transparent-transmission LoRa module. Based on the available E32-900T20D documentation reviewed during Phase 1, this module does not appear to expose per-packet RSSI over the transparent UART interface.

Newer EBYTE E32-xxxT20x V8.2 documentation lists AT commands for RSSI-related settings:

- `AT+DRSSI=data_rssi`
- `AT+DRSSI=1`
- `AT+DRSSI=?`
- `AT+ERSSI=erssi`
- `AT+ERSSI=1`
- `AT+ERSSI=?`

These commands were tested experimentally with both project E32-900T20D modules in configuration / sleep mode, with M0 and M1 tied HIGH to 3.3V. Both modules responded to AT traffic, but both rejected the RSSI commands with ERR-style responses.

Observed Seeker-side module result:

```text
AT+HELP=? -> produced response, but did not list AT+DRSSI or AT+ERSSI
AT+DRSSI=? -> =ERR
AT+DRSSI=1 -> =ERR
AT+ERSSI=? -> =ERR
AT+ERSSI=1 -> =ERR
```

The Seeker-side `AT+HELP=?` command listed:

```text
AT+DEVTYPE
AT+FWCODE
AT+UART
AT+RATE
AT+PACKET
AT+POWER
AT+TRANS
AT+ADDR
AT+CHANNEL
AT+IAP
AT+RESET
AT+SWITCH
AT+DEFAULT
AT+URXT
AT+AUXISIP
AT+WTIME
AT+FEC
AT+IODRIVE
AT+DUTYTX
AT+UAUX
```

Observed Hider-side module result:

```text
AT+DRSSI=? -> =ERR
AT+DRSSI=1 -> =ERR
AT+ERSSI=? -> =ERR
AT+ERSSI=1 -> =ERR
```

Conclusion for the current modules: do not assume received-data RSSI is available. The current E32-900T20D modules did not accept `AT+DRSSI=1` during this test.

Because of that, the current Seeker firmware prints:

```text
rssi=NA
```

For Phase 1 and early foxhunt-style testing, the Seeker uses proxy tracking metrics instead:

- `heard`: number of Hider beacon packets received
- `missed`: sequence gaps between Hider packets
- `last_hider_age_ms`: time since the last received Hider packet
- packet receive consistency over time

These metrics can support a rough hotter/colder search pattern, but they are not equivalent to true RSSI. For real RSSI-based direction or distance estimation in a later phase, the project will likely need one of these options:

- a LoRa radio/module and library that exposes raw SX127x RSSI values, such as an SPI SX1276/RFM95-style board
- an EBYTE UART module model that explicitly supports RSSI reporting
- a separate receiver/radio path dedicated to signal-strength measurements

## E32 LoRa RX Voltage Divider Fix

When wiring the E32 LoRa module to an Arduino Uno R3, the Arduino D11 TX signal is 5V logic, but the E32 RX input should not be driven directly with 5V. EBYTE documentation warns that 5V communication lines can risk damage, and other E32 Arduino guides recommend using a voltage divider or level shifter for the module RX line.

The original divider had the resistor values reversed:

```text
Arduino D11 -> 2k ohm -> E32 RX
E32 RX -> 1k ohm -> GND
```

That only produces about 1.67V:

```text
5V x (1k / (2k + 1k)) = 1.67V
```

The corrected divider is:

```text
Arduino D11 -> 1k ohm -> E32 RX
E32 RX -> 2k ohm -> GND
```

That produces about 3.33V:

```text
5V x (2k / (1k + 2k)) = 3.33V
```

Final breadboard fix:

- Arduino D11 jumper goes to F20.
- 1k ohm resistor goes from G20 to G26.
- 2k ohm resistor goes from H26 to right - rail.
- E32 RX remains at J26.

Because row 26 on the right side connects F26-G26-H26-I26-J26, row 26 becomes the safe 3.3V-ish signal node feeding E32 RX.

After this resistor fix, both boards showed successful E32 AUX transmit activity:

```text
TX AUX after: HIGH/READY busy pulse seen: YES
```

The Seeker also began receiving Hider packets:

```text
RX: MP1,HIDER01,BEACON,...
```
