# Hardware validation checklist

Run on one classic ESP32 target and one ESP32-S3 target before production rollout.

## Build and preparation

1. Build release with `python scripts/build_matrix.py --check`.
2. Build test firmware with `PLATFORMIO_BUILD_FLAGS="-DFW_LOG_LEVEL=3"`.
3. Save complete serial output at 115200 baud. Record board, connected sensors and firmware SHA.

## Smoke tests

- Erase EEPROM. Confirm AP name/password derivation and all configuration endpoints/responses in
  `FIRMWARE_CONTRACTS.md`.
- Configure WiFi. Reboot twice. Confirm SSID, topic, firmware name, low-power flag and relay states persist;
  confirm passwords are always masked.
- Exercise every MQTT command: `on`, `of`, `on2`, `of2`, `onon`, `ofof`, `onof`, `ofon`, `low1`, `low0`, `reset`.
- Compare MQTT JSON keys, units, retain flag and QoS with a pre-refactor capture.
- Disconnect network: confirm SPIFFS/SD save. Reconnect: confirm retained-data replay and file deletion only after publish.
- Test HTTP OTA with same/new version and SD OTA with valid/invalid `.bin`; confirm one OTA check line
  and verify `VERSION_OFFSET` contains the new server version or SD basename before reboot.
- Confirm failed OTA and firmware names longer than 21 characters leave `VERSION_OFFSET` unchanged.
- Confirm `/set_sensors` sends non-empty `sensors`/`info`, includes `board`, and logs route/status/body.
- Test time sources separately: HTTP, NTP fallback, RTC fallback, `/e.txt` fallback.
- Disconnect/reconnect each available I2C sensor; confirm recovery, hot-plug reinit and no mutex watchdog reset.
- Confirm always-on sniffer, 30-second window, device count, 1.30 multiplier and no increase in dropped packets.
- Enable low-power mode; confirm five-minute cadence, offline/online alternation and remaining-time deep sleep.

## Soak gate

- Run at least 200 monitoring cycles or 24 hours.
- Keep network unavailable for part of run, then restore it; include at least one MQTT and one diagnostic upload.
- Analyze log:

  `python scripts/analyze_soak_log.py serial.log --min-samples 20`

Pass conditions: heap span below 1 KB, each task watermark at least 2 KB, no watchdog restart,
no permanent I2C lock and no data loss beyond pre-refactor sniffer baseline.
