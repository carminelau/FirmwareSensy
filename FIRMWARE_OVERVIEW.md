# FirmwareSensy — Documentazione ad alto livello / High-Level Documentation

## Italiano

### Panoramica
FirmwareSensy è un firmware modulare per dispositivi ESP32, progettato per il monitoraggio ambientale e l’acquisizione dati da molteplici sensori. Supporta diverse varianti hardware tramite configurazioni in `platformio.ini`, un’architettura a task FreeRTOS e una doppia modalità operativa: **monitoring** (sensori + MQTT) e **sniffer** (analisi pacchetti WiFi).

### Obiettivi principali
- Acquisire dati da sensori ambientali (PM, CO₂, gas, meteo, luce, GPS, ecc.).
- Pubblicare dati via MQTT in formato JSON compatto.
- Gestire configurazioni persistenti via EEPROM.
- Consentire aggiornamenti firmware OTA e da SD.
- Operare in modalità sniffer per analisi WiFi.

### Funzionalità principali (dettaglio)
- **Gestione sensori multi-bus**: I2C, UART/Seriale, Modbus RTU, SPI.
- **Modalità operative**: Monitoring (sensori + MQTT) e Sniffer (WiFi promiscua).
- **Sincronizzazione e task**: FreeRTOS con task dedicati e semafori di sincronizzazione.
- **MQTT**: pubblicazione dati e ascolto comandi (topic listen).
- **JSON compatto**: buffer ridotto (circa 512 byte) per payload leggeri.
- **NTP/Time**: sincronizzazione oraria per timestamp coerenti.
- **Storage**: logging su SD/SPIFFS per backup locale.
- **WiFi**: gestione connessione, AP di configurazione e riconnessione automatica.
- **LED di stato**: indicazioni visive per stato dispositivo e fase operativa.
- **Aggiornamenti**: OTA e update da SD.

### Sensori integrati (librerie già presenti nel firmware)
**Qualità aria e particolato**
- Sensirion **SPS30** (PM)
- Sensirion **SEN5x/SEN55** (PM, VOC, NOx)
- Famiglia **PMSA003 / PMS5003** (PM)

**CO₂ / Gas**
- Sensirion **SCD30** (CO₂, T, RH)
- Sensirion **SCD4x (SCD40/SCD41)** (CO₂, T, RH)
- **Grove Multichannel Gas** (CO, NO₂, VOC, NH₃)
- **DFRobot MICS** (MICS-6814, gas multipli)
- **DFRobot Ozone Sensor** (O₃)

**Meteo e ambiente**
- **SHT21** (T, RH)
- **BH1750** (luce)

**GPS**
- **u-blox GNSS** (SparkFun u-blox GNSS library)

**Modbus / RS485**
- **ModbusSensorLibrary** (sensori Modbus RTU, es. anemometro/sonde esterne)

> Nota: l’abilitazione reale dei sensori dipende dai flag runtime e dalla variante hardware in `platformio.ini`.

### Architettura a due core (astrazione)
L’ESP32 utilizza due core. Il firmware separa le responsabilità per ridurre blocchi e migliorare la reattività.

**Core 0 — Servizi di base e attività reattive**
- Gestione LED di stato e segnalazioni visive.
- Supporto alla connettività e servizi di basso livello.
- Task di supporto che non devono bloccare il core principale.

**Core 1 — Logica applicativa e acquisizione dati**
- Loop principale del firmware.
- Lettura periodica sensori e composizione JSON.
- Pubblicazione MQTT e logging su SD/SPIFFS.
- Task specializzati: monitoring e sniffer.

> Nota: I task principali sono `loop_0_core`, `loop_1_core`, `loop_monitoring` e `loop_sniffer`, con priorità e stack configurati in `main.h`.

### Modalità operative
**1) Monitoring**
- Lettura dei sensori abilitati (runtime flag).
- Aggregazione dati in JSON (buffer limitato ~512 bytes).
- Pubblicazione via MQTT e/o salvataggio su SD/SPIFFS.

**2) Sniffer**
- Attivazione modalità promiscuous WiFi.
- Channel hopping e conteggio dispositivi/packets.
- Output statistico via MQTT.

### Persistenza e configurazione
- EEPROM per credenziali WiFi, flag modalità, topic MQTT.
- Funzioni chiave: `read_conf_eeprom()`, `read_wifi_eeprom()`, `write_inside_eeprom()`.

### Aggiornamenti firmware
- OTA con verifica versione e download remoto.
- Update da SD con ricerca file `.bin`.

### Build e varianti hardware
- Configurazioni per più board in `platformio.ini`.
- Pin e feature flag definiti tramite `build_flags`.

---

## English

### Overview
FirmwareSensy is a modular firmware for ESP32 devices designed for environmental monitoring and multi-sensor data acquisition. It supports multiple hardware variants via `platformio.ini`, a FreeRTOS task-based architecture, and two runtime modes: **monitoring** (sensors + MQTT) and **sniffer** (WiFi packet analysis).

### Main goals
- Acquire data from environmental sensors (PM, CO₂, gas, weather, light, GPS, etc.).
- Publish data via MQTT using compact JSON payloads.
- Persist configuration via EEPROM.
- Support OTA and SD-based firmware updates.
- Enable WiFi sniffer mode for analytics.

### Main features (detailed)
- **Multi-bus sensor management**: I2C, UART/Serial, Modbus RTU, SPI.
- **Operating modes**: Monitoring (sensors + MQTT) and Sniffer (WiFi promiscuous).
- **FreeRTOS synchronization**: dedicated tasks and semaphores.
- **MQTT**: data publishing and command listening (listen topic).
- **Compact JSON**: small payload buffer (about 512 bytes).
- **NTP/Time**: time sync for consistent timestamps.
- **Storage**: SD/SPIFFS logging for local backup.
- **WiFi**: connection handling, configuration AP, auto-reconnect.
- **Status LED**: visual state indication.
- **Updates**: OTA and SD-based update.

### Integrated sensors (libraries included in firmware)
**Air quality & particulate matter**
- Sensirion **SPS30** (PM)
- Sensirion **SEN5x/SEN55** (PM, VOC, NOx)
- **PMSA003 / PMS5003** family (PM)

**CO₂ / Gas**
- Sensirion **SCD30** (CO₂, T, RH)
- Sensirion **SCD4x (SCD40/SCD41)** (CO₂, T, RH)
- **Grove Multichannel Gas** (CO, NO₂, VOC, NH₃)
- **DFRobot MICS** (MICS-6814, multi-gas)
- **DFRobot Ozone Sensor** (O₃)

**Weather & environment**
- **SHT21** (T, RH)
- **BH1750** (light)

**GPS**
- **u-blox GNSS** (SparkFun u-blox GNSS library)

**Modbus / RS485**
- **ModbusSensorLibrary** (Modbus RTU sensors, e.g. anemometer/external probes)

> Note: actual sensor enablement depends on runtime flags and the specific hardware variant in `platformio.ini`.

### Dual-core architecture (abstraction)
ESP32 provides two cores. The firmware splits responsibilities to minimize blocking and improve responsiveness.

**Core 0 — Base services and reactive tasks**
- Status LED control and visual signaling.
- Connectivity support and low-level services.
- Helper tasks that should not block the main application flow.

**Core 1 — Application logic and data acquisition**
- Main firmware loop.
- Periodic sensor reads and JSON composition.
- MQTT publishing and SD/SPIFFS logging.
- Specialized tasks: monitoring and sniffer.

> Note: Main tasks include `loop_0_core`, `loop_1_core`, `loop_monitoring`, and `loop_sniffer`, with priorities and stack sizes configured in `main.h`.

### Operating modes
**1) Monitoring**
- Reads enabled sensors (runtime flags).
- Aggregates data into JSON (buffer ~512 bytes).
- Publishes via MQTT and/or logs to SD/SPIFFS.

**2) Sniffer**
- Enables WiFi promiscuous mode.
- Channel hopping and device/packet counting.
- Publishes statistics over MQTT.

### Persistence and configuration
- EEPROM stores WiFi credentials, mode flags, MQTT topic.
- Key functions: `read_conf_eeprom()`, `read_wifi_eeprom()`, `write_inside_eeprom()`.

### Firmware updates
- OTA with version check and remote download.
- SD update by scanning for `.bin` files.

### Build and hardware variants
- Multiple board environments in `platformio.ini`.
- Pins and features via `build_flags`.
