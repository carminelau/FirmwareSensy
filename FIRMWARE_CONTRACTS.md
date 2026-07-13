# FirmwareSensy compatibility contracts

Refactor may change internals, not contracts below.

## Hardware environments

Eight production targets remain buildable with same names, pins, flash layouts and relay flags:
`sensy_2021_V4_white`, `sensy_2023_V1_green`, `sensy_2023_V2_black`,
`sensy_2024_V1_green`, `sensy_2024_V2_ENEA`, `sensy_2024_V3_red`,
`sensy_2024_V4_green`, `sensy_2024_V4_black`.

## EEPROM layout

- `0`: SSID string; `32`: password string.
- `97`: WiFi flag; `101`: configured flag.
- `104`: firmware filename/version; `126`: MQTT device topic.
- `205`: low-power flag; `206`: reserved legacy sniffer flag, retained but ignored.
- `207` and `208`: relay 1/2 state.
- `511`: initialization marker, value `0xAA`; EEPROM size `512` bytes.

No address, type or initialization behavior may change without explicit migration.

## Network contracts

Configuration AP endpoints and exact response bodies:

- `GET /status` → `ok`
- `GET /reset` → `ok`
- `GET /isGPS` → `true` or `false`
- `GET /getMacAddress` → 12-character device identifier
- `GET /scanWifi` → JSON array containing `ssid` and `rssi`
- `POST /configWifi`, form fields `ssid` and `pwd` → `connected`, `error`, or `errorNOSSID`

Backend resources remain `/epoch`, `/get_ID`, `/get_nearest_data`,
`/versione_firmware`, `/aggiornamento_firmware` and `/set_sensors`. Diagnostics sends
the documented `sensors`, `ID`, `versione`, `board` and `info` query fields.

## MQTT and payloads

- Publish topic: stored device `topic`; command topic: `topic + "GESTORE"`.
- Data publish stays retained QoS 2; stored-data replay stays retained QoS 1.
- Commands remain exact and case-sensitive: `on`, `of`, `on2`, `of2`, `onon`, `ofof`,
  `onof`, `ofon`, `low1`, `low0`, `reset`.
- Sensor JSON keeps existing keys and units, including `timestamp`, location/GPS fields,
  PM fields, temperature/humidity, gas fields, CO2, wind, soil, luminosity and
  `num_devices_sniffed`.

## Runtime schemes

- Time fallback order: HTTP server → NTP → DS1307 RTC → `/e.txt` local storage.
- Sniffer is always active. Capacity: 512 devices; active window: 30 seconds; output multiplier: 1.30.
- Low-power cycle: five-minute total cadence, alternating offline/local-save and
  online/MQTT cycles, followed by deep sleep for remaining cycle time.
- Sensor diagnostics: startup and every 20 monitoring cycles. OTA: every 5 cycles.
  RTC refresh: startup and every 15 cycles.
- After a successful HTTP or SD OTA, the new firmware filename is written and verified at
  `EEPROM_ADDR::VERSION_OFFSET` before reboot. Failed/incomplete updates never change it.
- Firmware filename maximum length is 21 characters, preserving `TOPIC_OFFSET` at address 126.
- SPIFFS/SD fallback, OTA-from-SD and watchdog/I2C recovery behavior remain enabled.

## Release/debug policy

- Default `FW_LOG_LEVEL=1`: saved configuration, sensor availability, cycle summaries,
  network outcomes, warnings and errors.
- `FW_LOG_LEVEL=3`: per-sensor sampling, values, final JSON and task stack high-water marks.
- Serial firmware and PlatformIO monitor use 115200 baud. Serial `RESET` stays active.
- `FW_LOG_BILINGUAL=1`: optional debug translation for critical messages, fixed buffers only.
- `ENABLE_MICS4514=0`: matches current disabled state; driver stays available at compile time.
