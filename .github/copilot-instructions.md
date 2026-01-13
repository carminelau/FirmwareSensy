<!-- Project-specific Copilot instructions for FirmwareSensy -->
# Quick orientation — FirmwareSensy

This file tells AI coding agents how this repo is organized and points out the project-specific patterns, build workflows, and conventions that make contributions safe and effective.

Key files
- `platformio.ini`: All board envs, pins and external libraries are declared here via `lib_deps` and `build_flags`. When adding a board or changing pins, update this file.
- `src/main.cpp`: Main firmware logic. Contains FreeRTOS tasks (`loop_0_core`, `loop_1_core`, `loop_monitoring`, `loop_sniffer`) and most sensor/workflow code.
- `src/main.h`: Global definitions, sensor structs, compile-time constants and shared globals (many names and comments are in Italian).
- `auto-upload.sh` / `auto-upload.bat`: Helper upload scripts (use on local machine for quick flashing).

Big-picture architecture
- Single firmware binary built for different ESP32 variants (see `[env:...]` entries in `platformio.ini`).
- Runtime uses FreeRTOS tasks pinned to cores. Do not convert logic to be fully blocking — prefer tasks or semaphores (example: `sweepDoneSem = xSemaphoreCreateBinary()` and `xSemaphoreGive`/`xSemaphoreTake`).
- Two major runtime modes: "sniffer" (WiFi packet sniffing/hopping) and "monitoring" (sensor sampling + MQTT). The mode is set at runtime (flags like `sniffer`, `conf`, `low`).
- Data flow: sensors -> JSON (`doc`) -> MQTT (`send_data_mqtt()`) or local storage (SPIFFS/SD). Watch JSON buffer sizes (`jsonOutput[512]`).

Build / upload / debug
- Build: `pio run -e <env>` (e.g. `pio run -e sensy_2024_V4_green`).
- Upload: `pio run -e <env> -t upload` or run `./auto-upload.sh` for the configured env.
- PlatformIO resolves many `lib_deps` (see `platformio.ini`). To add a library, add it to the correct `[env:...]` stanza.
- Serial debug: firmware uses `Serial.begin(9600)` extensively. Use the serial monitor to validate behavior and to follow descriptive prints.

Important project-specific conventions
- Hardware selection via `build_flags`: pins and feature flags are compile-time macros (e.g. `-DLED_TYPE=2`, `-DYEAR=2024`, `-DVERSION=4`). Prefer editing `platformio.ini` for board-level changes.
- Runtime feature flags: many booleans (e.g. `sps`, `scd30`, `gas`) are checked at runtime to enable sensor reads. Search for `read_*` functions (e.g. `read_scd30`, `read_multigas`) when modifying sensor behavior.
- EEPROM & persistence: configuration and WiFi creds are read/written using functions like `read_conf_eeprom()`, `read_wifi_eeprom()`, `write_inside_eeprom()` — inspect other source files for implementations before changing formats.
- OTA/update paths: code supports OTA checks (`check_update_OTA()`) and SD update from `.bin` files (`find_first_bin_file` + `updateFromFile`). Keep firmware name (`nameBinESP`) stable when changing OTA logic.

Memory / safety notes
- JSON buffers are small (`512` bytes). Avoid emitting large or deeply nested JSON objects; prefer incremental writes or storage when collecting many fields.
- Tasks and semaphores coordinate long-running operations (sweeps, monitoring). When adding blocking code, prefer creating a task or yield frequently using `vTaskDelay()`.

Useful search patterns for contributors
- Find config/EEPROM code: `read_conf_eeprom|read_wifi_eeprom|write_inside_eeprom`.
- Find MQTT/topic code: search `topic`, `topicListen`, and `send_data_mqtt()`.
- Find sensor init/read functions: `init_`, `read_`, `print_` prefixes (e.g. `init_gps`, `read_sps30`, `read_pmsA003`).

Style and language
- Code comments and many identifiers use Italian—keep new comments consistent in Italian where appropriate, or add short bilingual notes if helpful.

If you edit behavior
- Update `platformio.ini` when changing pins, board or libs.
- Run `pio run` locally and test with the serial monitor before proposing PRs.
- Keep changes minimal and focused: this repo uses a single large `main.cpp` cover many responsibilities. Prefer small, well-scoped PRs (e.g., "add debug log for SCD30" or "move sensor calibration to helper").

Where to look next
- Search the repo for other .cpp/.h files that implement helpers referenced above (EEPROM, WiFi, MQTT, file update). Use the function names above to locate implementations.

Questions for the maintainers
- Should new features be guarded by compile-time flags or enabled at runtime via EEPROM? (I left both patterns alone.)
- Preferred language for new comments (Italian vs English)?

If anything here is unclear or you'd like expanded examples (e.g. how to add a new sensor driver), tell me which area and I'll iterate.
