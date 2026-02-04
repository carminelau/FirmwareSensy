#pragma once

/**********************************
 *        LIBRERIE INCLUSE        *
 **********************************/
// Core
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <time.h>
#include <TimeLib.h>
#include "esp_task_wdt.h"
#include <RTClib.h>  // RTC I2C support

// Sensori
#include "SHT21.h"
#include <Multichannel_Gas_GMXXX.h>
#include "DFRobot_OzoneSensor.h"
#include "DFRobot_MultiGasSensor.h"
#undef DBG  // Undefine macro conflittuale prima di includere MICS
#include "PMserial.h"
#include "DFRobot_MICS.h"
#include <sps30.h>
#include "SensirionI2CSen5x.h"
#include <SensirionI2cScd30.h>
#include <SensirionI2cScd4x.h>
#include <BH1750.h>
#include <ModbusSensorLibrary.h>

// GPS
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>

// WiFi / MQTT / HTTP
#include "WiFi.h"
#include <WiFiClient.h>
#include <MQTT.h>
#include <ArduinoHttpClient.h>
#include "HTTPClient.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Web Server
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// OTA
#include <Update.h>

// File system
#include "FS.h"
#include "SPIFFS.h"
#include "SD.h"
#include "SPI.h"

// JSON
#include "ArduinoJson.h"

// Libreria personalizzata
//#include "hywdc6e-lib.h"

// STL
#include <vector>

/**********************************
 *      DEFINIZIONI GENERALI      *
 **********************************/
#define SP30_COMMS I2C_COMMS
#define MODBUSRTU_REDE
#define SCD30_I2C_ADDR_61 0x61
#define COLLECT_NUMBER 20
#define Ozone_IICAddress ADDRESS_3
#define SLAVE_ID 1
#define REG_BASE 0
#define REG_COUNT 38
#define uS_TO_S_FACTOR 1000000
#define TIME_TO_SLEEP 540

#if defined(SENSY_2021_V4_WHITE)
#define RESPONSE_TIMEOUT 5000
#endif

// MICS4514
#define CALIBRATION_TIME 1
#define MICS_I2C_ADDRESS MICS_ADDRESS_0

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

/**
 * Configurazione FreeRTOS Task
 * Core allocation e task priorities
 */
namespace TASK_CONFIG 
{
    // Core assignment
    static constexpr BaseType_t CORE_0 = 0;
    static constexpr BaseType_t CORE_1 = 1;
    
    // Priority levels (0=lowest, 25=highest)
    static constexpr UBaseType_t PRIORITY_IDLE = 0;
    static constexpr UBaseType_t PRIORITY_LOW = 1;
    static constexpr UBaseType_t PRIORITY_NORMAL = 2;
    static constexpr UBaseType_t PRIORITY_HIGH = 3;
    static constexpr UBaseType_t PRIORITY_VERY_HIGH = 4;
    
    // Stack sizes in bytes
    static constexpr uint32_t STACK_SIZE_SMALL = 2048;
    static constexpr uint32_t STACK_SIZE_NORMAL = 4096;
    static constexpr uint32_t STACK_SIZE_LARGE = 8192;
    static constexpr uint32_t STACK_SIZE_MONITOR = 12288;  // Monitor task needs more memory
}

/**
 * Timing configuration per Debug
 */
struct DebugTimestamps
{
    unsigned long cycleStartMs = 0;
    unsigned long sweepStartMs = 0;
    unsigned long lastSweepMs = 0;
    unsigned long monitorStartMs = 0;
    unsigned long lastMonitorMs = 0;
};
extern DebugTimestamps debugTs;

// Global task handles (usabili come &Task0 in setup)
extern TaskHandle_t Task0_handle;
extern TaskHandle_t Task1_handle;
extern TaskHandle_t Task2_handle;

/**********************************
 *     STRUTTURE DATI SENSORI     *
 **********************************/
// Anemometro
struct dataset_t
{
    uint16_t DeviceState;
    uint16_t WindDirection;
    float WindSpeed;
    float Temperature;
    float Humidity;
    float AirPressure;
    uint16_t CompassHeading;
    uint16_t PrecipitationType;
    float PrecipitationIntensity;
    float AccumulatedPrecepitation;
    uint16_t IntensityUnit;
    uint16_t GpsStatus;
    float GpsSpeed;
    uint16_t GpsHeading;
    float Longitude;
    float Latitude;
    float DiustConcentration;
    float Visibility;
    float Luminance;
    float AccumulatedSolarRadiation;
    float RadiationPower;
    uint16_t CompassCorrectedWindDirection;
    float Altitude;
};

/**********************************
 *   DICHIARAZIONI GLOBALI VARI   *
 **********************************/
bool arrived = false;
bool daresettare = false;
bool sd = false;
bool connected = false;
bool conf = false;
bool AP = false;
bool wifi = false;
bool justConfigured = false;  // Flag per inviare diagnostica subito dopo configurazione

char myConcatenation[12];

String topic = "";
String nameBinESP = "";
String topicListen;
String timezone_it = "CET-1CEST,M3.5.0,M10.5.0/3";
const char *verionBoard = STR(YEAR) "V" STR(VERSION);

/**********************************
 *     VARIABILI DI STATO         *
 **********************************/
bool sps, ozone, pmsa003, gas, co_hd, no2_hd, o3_hd, so2_hd, sht, ane, low, lux, soil, sniffer;
bool sen55, scd30, scd41, mics4514;
bool accesspoint, relay1, relay2;

int saveCounterSD = 0;
int saveCounterSPIFFS = 0;
int epochs = 0;
int resetCount = 0;

volatile int state = LOW;
volatile int state_short = LOW;
volatile int state_long = LOW;
volatile unsigned long current_high;
volatile unsigned long current_low;

/**********************************
 *     OGGETTI SENSORI E HW       *
 **********************************/
AsyncWebServer server(80);
BH1750 lightMeter;

// Sensori Multigas - indirizzo I2C configurato nel costruttore
DFRobot_GAS_I2C co_hd_sensor(&Wire, 0x74);  // CO_HD sensor @ 0x74
DFRobot_GAS_I2C no2_hd_sensor(&Wire, 0x76); // NO2_HD sensor @ 0x76
DFRobot_GAS_I2C o3_hd_sensor(&Wire, 0x75);  // O3_HD sensor @ 0x75
DFRobot_GAS_I2C so2_hd_sensor(&Wire, 0x77); // SO2_HD sensor @ 0x77

// RTC I2C - DS1307
RTC_DS1307 rtc_i2c;
// Semaphore signaled when WiFi is connected via AP/web config or auto-connect
extern SemaphoreHandle_t wifiConnectedSem;
// Debug flag: if true, xSemaphoreGive for wifiConnectedSem will be skipped (useful for isolation)
extern bool debugDisableWifiSem;
// Queue for delegating wifi-connected events from ISRs/handlers to notifier task
extern QueueHandle_t wifiEventQueue;
// Task handle for notifier task
extern TaskHandle_t wifiNotifierTaskHandle;
// Task handle for sniffer manager (loop_0_core)
extern TaskHandle_t cycleTaskHandle;
// Task handle for persistent monitor task
extern TaskHandle_t monitorTaskHandle;
// pending reboot to apply sniffer EEPROM change
extern bool pendingSnifferReboot;

// Runtime control helpers
void start_sniffer_manager();
void stop_sniffer_manager();

DFRobot_MICS_I2C mics(&Wire, MICS_I2C_ADDRESS);
DFRobot_OzoneSensor Ozone;
GAS_GMXXX<TwoWire> sensore;
SerialPM pms(PMSA003, RX_PMS_PIN, TX_PMS_PIN);
SHT21 sht21;
SensirionI2CSen5x sen5x;
SensirionI2cScd30 scd3x;
SensirionI2cScd4x scd4x;
ModbusSensorLibrary sensors(RO_PIN, DI_PIN, DE_PIN, RE_PIN);

std::vector<String> Pollutants;
std::vector<String> PollutantsMissing;

SFE_UBLOX_GNSS myGNSS;
double latitude = 0;
double longitude = 0;
byte SIV;

SPIClass spi = SPIClass();
WiFiClient clientWifi;
WiFiClient clientota;
MQTTClient clientMQTT(512);
/**********************************
 *   SENSOR DATA & MISURE RAW     *
 **********************************/
// Ozone
float O3 = 0.0;
float adc1_O3, adc2_O3;

// Multigas (0x08)
float no2, c2h5oh, voc, co;
float nh3;

// CO_HD Sensor (0x74)
float co_hd_ppm = 0.0;
float co_hd_temp = 0.0;

// NO2_HD Sensor (0x76)
float no2_hd_ppm = 0.0;
float no2_hd_temp = 0.0;

// O3_HD Sensor (0x75)
float o3_hd_ppm = 0.0;
float o3_hd_temp = 0.0;

// SO2_HD Sensor (0x77)
float so2_hd_ppm = 0.0;
float so2_hd_temp = 0.0;

// Calibration constants from PDF
float GM102B_init = 1.41, GM102B_dV = -1.03, GM102B_ppm = 5;
float GM302B_init = 0.94, GM302B_dV = -0.46, GM302B_ppm = 50;
float GM502B_init = 1.42, GM502B_dV = -0.89, GM502B_ppm = 50;
float GM702B_init = 1.22, GM702B_dV = 0.87, GM702B_ppm = 150;

// PMS/SPS
float pmAe1_0, pmAe2_5, pmAe10_0;
float pmAe1_0Test, pmAe2_5Test, pmAe10_0Test;

// SEN55
float sen55_temp = 0.0;
float sen55_hum = 0.0;
float no2_index = 0;
float voc_index = 0;

// SCD30
float scd30_temp = 0.0;
float scd30_hum = 0.0;
float scd30_co2 = 0.0;

// SCD41
float scd41_temp = 0.0;
float scd41_hum = 0.0;
uint16_t scd41_co2 = 0;

// Anemometro
short windDirection_ane;
float windSpeed_ane;
float temperature_ane;
float humidity_ane;
float pressure_ane;

// Soil Moisture
float soil_ph = 0.0;
float soil_temperature = 0.0;
float soil_humidity = 0.0;
int soil_conductivity = 0.0;
float soil_nitrogen = 0.0;
float soil_phosphorus = 0.0;
float soil_potassium = 0.0;

/**********************************
 *         GESTIONE JSON          *
 **********************************/
JsonDocument checkSensor;
JsonDocument doc;
JsonDocument info;
char stringCheckSensor[512];
char stringInfo[512];
char jsonOutput[512];

String jsonWifi = "";

/**********************************
 *     CONFIGURAZIONE RETE        *
 **********************************/
const char *ntpServer = "pool.ntp.org";
const char *ntpServer2 = "ntp.unisa.it";
char ssidAP[18];
char psswdAP[10];
char psswdAPssid[10];
String eeprom_ssid = "";
String eeprom_psswd = "";
bool arrivedlow = false;

/**********************************
 *        SERVER/MQTT             *
 **********************************/
const char *mqtt_server = "sensy.sensesquare.eu";
int portaMQTT = 1883;
String host = "sensy.sensesquare.eu";
int port = 5000;
long contentLength = 0;
bool isValidContentType = false;
bool GPSsensor = false;

/**********************************
 *   TABELLE CALIBRAZIONE GAS     *
 **********************************/
// NO2 – GM102B
const float gm102b_rh_offset[4][7] PROGMEM = {
    {-10.0, 0.0, 10.0, 20.0, 30.0, 40.0, 50.0},
    {1.71, 1.58, 1.45, 1.39, 1.12, 1.00, 0.89},
    {1.49, 1.32, 1.28, 1.08, 0.99, 0.88, 0.71},
    {1.28, 1.15, 10.9, 0.90, 0.86, 0.71, 0.68}};

const float gm102b_u2gas[2][12] PROGMEM = {
    {0.0, 0.21, 0.39, 0.7, 0.95, 1.15, 1.35, 1.45, 1.6, 1.69, 1.79, 1.81},
    {0.0, 0.5, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0}};

// Ethanol – GM302B
const float gm302b_rh_offset[4][13] PROGMEM = {
    {-10.0, -5.0, 0.0, 5.0, 10.0, 15.0, 20.0, 25.0, 30.0, 35.0, 40.0, 45.0, 50.0},
    {1.71, 1.61, 1.58, 1.50, 1.42, 1.30, 1.25, 1.18, 1.15, 1.12, 1.00, 0.92, 0.88},
    {1.45, 1.36, 1.33, 1.28, 1.20, 1.11, 1.08, 1.00, 0.98, 0.95, 0.85, 0.79, 0.73},
    {1.27, 1.20, 1.18, 1.10, 1.05, 0.95, 0.92, 0.88, 0.86, 0.81, 0.72, 0.69, 0.64}};

const float gm302b_u2gas[2][11] PROGMEM = {
    {1.25, 1.5, 2.0, 2.25, 2.5, 3.1, 3.3, 3.6, 3.7, 3.8, 3.85},
    {0.0, 5.0, 10.0, 15.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0}};

const float gm502b_rh_offset[4][13] PROGMEM = {
    {-10.0, -5.0, 0.0, 5.0, 10.0, 15.0, 20.0, 25.0, 30.0, 35.0, 40.0, 45.0, 50.0},  // °C
    {1.71, 1.62, 1.54, 1.50, 1.42, 1.30, 1.25, 1.16, 1.14, 1.11, 1.00, 0.92, 0.88}, // Rs/R0 @ 30%RH
    {1.45, 1.38, 1.35, 1.28, 1.21, 1.11, 1.08, 1.00, 0.98, 0.96, 0.85, 0.79, 0.75}, // Rs/R0 @ 60%RH
    {1.25, 1.20, 1.18, 1.10, 1.05, 0.95, 0.92, 0.88, 0.86, 0.81, 0.73, 0.68, 0.62}  // Rs/R0 @ 85%RH
};

const float gm502b_u2gas[2][9] PROGMEM = {
    {2.52, 2.90, 3.20, 3.40, 3.60, 3.90, 4.05, 4.15, 4.20}, // Alcohol [V]
    {0.0, 1.0, 3.5, 5.0, 10.0, 30.0, 50.0, 80.0, 100.0}     // VOC [ppm]
};

// Tabelle di offset per GM702B in funzione di temperatura e umidità relativa
const float gm702b_rh_offset[4][7] PROGMEM = {
    {-10.0, 0.0, 10.0, 20.0, 30.0, 40.0, 50.0}, // °C
    {1.71, 1.58, 1.45, 1.38, 1.13, 1.01, 0.88}, // Rs/R0 @ 30%RH
    {1.47, 1.32, 1.28, 1.08, 0.98, 0.88, 0.72}, // Rs/R0 @ 60%RH
    {1.28, 1.15, 1.08, 0.90, 0.87, 0.71, 0.68}  // Rs/R0 @ 85%RH
};

// Tabella di conversione tensione-CO (ppm) per GM702B
const float gm702b_u2gas[2][9] PROGMEM = {
    {0.25, 0.65, 0.98, 1.35, 1.8, 1.98, 2.1, 2.38, 2.42},     // V
    {0.0, 5.0, 10.0, 20.0, 50.0, 100.0, 160.0, 500.0, 1000.0} // CO [ppm]
};

// ===== CONFIGURAZIONE =====
extern const int HOP_INTERVAL_MS;      // ms per canale
extern const int CHANNEL_MIN;
extern const int CHANNEL_MAX;
extern const unsigned long PRINT_INTERVAL_MS;

// Soglie per sessioni / filtri
extern const uint32_t INACTIVITY_TIMEOUT_MS;
extern const uint32_t ACTIVE_WINDOW_MS;
extern const uint32_t FIXED_THRESHOLD_MS;
extern const uint32_t DEVICE_FORGET_MS;

// Limiti tabella
#define MAX_DEVICES 512

// ===== Strutture =====
struct DeviceInfo {
  uint8_t mac[6];
  int lastRssi;
  uint32_t lastSeen;       // millis
  uint32_t firstSeen;      // millis (quando è stato creato il record)
  uint32_t count;          // numero di pacchetti visti
  int firstChannel;

  // Campi per sessione / accumulo tempo
  uint32_t sessionStart;   // millis, 0 se non in sessione
  uint32_t cumulativeSeenMs; // ms totali accumulati da sessioni chiuse
};

typedef struct {
  uint8_t mac[6];
  int8_t rssi;
  uint8_t channel;
  uint8_t frame_type;
  uint8_t frame_subtype;
  uint32_t ts;
} SniffMsg;

// Stato globale (definiti in main.cpp)
extern DeviceInfo devices[MAX_DEVICES];
extern int devicesCount;
extern portMUX_TYPE devicesMux;
extern QueueHandle_t pktQueue;
extern volatile uint32_t droppedPackets;
// ===== CONFIGURAZIONE =====
const int HOP_INTERVAL_MS = 200;
const int CHANNEL_MIN = 1;
const int CHANNEL_MAX = 13;
const unsigned long PRINT_INTERVAL_MS = 30000;

const uint32_t INACTIVITY_TIMEOUT_MS = 60 * 1000UL;
const uint32_t ACTIVE_WINDOW_MS = 120 * 1000UL;
const uint32_t FIXED_THRESHOLD_MS = 10 * 60 * 1000UL;
const uint32_t DEVICE_FORGET_MS = 24UL * 60 * 60 * 1000UL;

DeviceInfo devices[MAX_DEVICES];
int devicesCount = 0;
portMUX_TYPE devicesMux = portMUX_INITIALIZER_UNLOCKED;

static QueueHandle_t _pktQueue = NULL;
QueueHandle_t pktQueue = NULL;
volatile uint32_t droppedPackets = 0;

// channel hop control
static TaskHandle_t channelHopHandle = NULL;
static volatile bool channelHopShouldStop = false;

typedef void (*mqtt_disconnect_cb_t)();
typedef void (*mqtt_reconnect_cb_t)();

// MQTT callbacks + saved creds per doFullSweepAndReconnect
static mqtt_disconnect_cb_t mqttDisconnectCb = NULL;
static mqtt_reconnect_cb_t mqttReconnectCb = NULL;

const unsigned long SWEEP_DURATION_MS = 20000UL; // 20s sweep

// flag condiviso monitorDone: il monitor imposta true quando ha finito la sua attività
static volatile bool monitorDone = false;
int num_devices_sniffed = 0;
int avg_time_per_device = 0;

// Timer per finestra temporale sniffer (30 secondi)
unsigned long snifferStartTime = 0;
const unsigned long SNIFFER_WINDOW_MS = 30000; // 30 secondi

// ============================================================================
// PROTOTIPI FUNZIONI (ORGANIZZATI PER SEZIONE)
// ============================================================================

// Utility generiche
int dir_wind_fix(int dire);
float round_float(float value);
bool find_arduino_devices();
String macToString(const uint8_t *mac);

// EEPROM / Configurazione
void write_inside_eeprom(bool val, int addr);
void read_topic_eeprom();
void read_version_eeprom();
bool read_sniffer_eeprom();
void write_sniffer_eeprom(bool val);
void write_conf_eeprom(bool b);
bool read_conf_eeprom();
bool read_wifi_eeprom();
void write_string_eeprom(char *c, int offset);
void write_string_eeprom(String c, int offset);
void write_low_eeprom(bool val);
bool read_low_eeprom();
bool read_relay1_eeprom();
void write_relay1_eeprom(bool val);
bool read_relay2_eeprom();
void write_relay2_eeprom(bool val);
void check_vergin_eeprom();
void delete_info_sensy();
void delete_wifi_settings();

// I2C e periferiche
void init_i2c();
bool check_i2c_bus_health();  // Verifica e recovery blocchi I2C
bool init_relay(int relayPin);

// Sensori multigas
bool init_multigas();
void read_multigas();
bool init_co_hd();
void read_co_hd();
bool init_no2_hd();
void read_no2_hd();
bool init_o3_hd();
void read_o3_hd();
bool init_so2_hd();
void read_so2_hd();
float correct_gas_value(float u, float temp, float humidity, float *u_corr, size_t size);
float return_ppm_gas_value(float u, float *u2gas, size_t size);
float get_no2_ppm(uint32_t raw, float temp, float humidity);
float get_c2h5oh_ppm(uint32_t raw, float temp, float humidity);
float get_voc_ppm(uint32_t raw, float temp, float humidity);
float get_co_ppm(uint32_t raw, float temp, float humidity);
bool init_mics();
float read_mics(uint8_t gasTypes, const char *gasNames);

// Sensori specifici
bool init_ozone();
int16_t read_ozone();
bool init_pmsA003();
void read_pmsA003();
bool init_sps30();
bool read_sps30(float *pm1, float *pm2, float *pm10);
void print_sps30_values(float pm1, float pm2, float pm10);
bool init_sen55();
bool read_sen55();
bool init_scd30();
bool read_scd30();
bool init_scd4x();
bool read_scd4x();
bool init_luxometer();
float read_luxometer();
void read_anemometer();
bool check_soil_moisture();
void read_soil_moisture();
void check_sensors_diagnostics();
void send_sensors_diagnostics();
float calibrate(float raw, float init, float dV, float ppm);

// GPS
bool init_gps();
void scan_i2c_devices();  // Debug: scansiona tutti i dispositivi I2C

// RTC e tempo
void init_rtc();
int get_epoch();
int get_epoch_storage();
unsigned long get_epoch_ntp_server();
void set_rtc(int timestamp);
void set_timezone(String timezone);
unsigned long get_time_with_hierarchy();  // Sincronizzazione con gerarchia di priorità

// RTC I2C (DS1307)
void init_rtc_i2c();
void set_rtc_i2c_time(unsigned long epoch);
unsigned long get_rtc_i2c_time();
void sync_rtc_i2c_with_ntp();
bool rtc_i2c_available();
bool rtc_i2c_lost_power();

// WiFi e Access Point
void connect_wifi_network();
void init_wifi();
void create_access_point();
void disconnect_access_point();
void get_mac_address();
String get_id_square();
String get_list_wifi(bool forceRefresh = false);

// MQTT
void init_mqtt();
void connect_mqtt_client();
void loop_mqtt();
bool send_data_mqtt();
void read_message_received_mqtt(String &topic, String &payload);
void delete_message_received_mqtt();
bool check_urgent_mqtt_command();
void check_reply_ID();

// OTA
String get_header_value(String header, String headerName);
void check_update_OTA();
void exec_update_OTA();

// Storage SPIFFS e SD
void init_spiffs();
String read_file_storage(fs::FS &fs, const char *path);
bool write_file_storage(fs::FS &fs, const char *path, const char *message);
void delete_file_storage(fs::FS &fs, const char *path);
void write_file_data(char *jsonString);
bool init_sd_card();
void send_data_from_storage(fs::FS &fs);
int get_count_data_saved(fs::FS &fs);
File find_bin_file(fs::FS &fs, const char *path);
File find_first_bin_file(fs::FS &fs, const char *directory, String &foundFilePath);
void updateFromFile(File updateBin, const char *filePath);

// Pulsanti
void read_reset_button();
void debounce_delay();
void handle_button_press();
void handle_button_release();
bool check_short_press();
bool check_long_press();
void press_short_time_button();
void press_long_time_button();
void check_pressing_button();

// Task paralleli
void loop_monitoring(void *pvParameters);
void loop_sniffer(void *pvParameters);
void loop_0_core(void *pv);
void loop_1_core(void *pv);

// API server / parsing
bool get_nearest_data(const String &params);
void parse_response(const String &payload);
void process_token(const String &token);
String vector_to_encoded_json_array(const std::vector<String> &vec);

// ===== Funzioni esportate (sniffer) =====

// Implementazione fornita in sniffer.cpp. Puoi registrarla con esp_wifi_set_promiscuous_rx_cb.
void IRAM_ATTR wifi_sniffer_packet(void* buf, wifi_promiscuous_pkt_type_t type);

// Inizializza internals dello sniffer (queue, task consumer, printer). Chiamare una volta in setup().
void snifferInit();

// Avvia lo sniffer in modalità "single-channel" (promiscuous sul canale corrente).
// Utile se vuoi mantenere la STA connessa e sniffare solo il canale corrente.
// Non avvia il channel hopper.
void startSnifferSingleChannel();

// Ferma lo sniffer (disabilita promiscuous e callback).
void stopSniffer();

// Avvia il task che esegue channel hopping (cambia canale periodicamente).
// startSnifferSingleChannel() deve essere chiamato prima per abilitare il ricevitore.
void startChannelHopTask();

// Ferma il task di channel hopping in modo sicuro.
void stopChannelHopTask();

// doFullSweepAndReconnect: esegue uno sweep "full" time-sliced.
// - Disconnette temporaneamente la STA (invocando la callback mqttDisconnect se registrata).
// - Attiva promiscuous + channel hop per sweepMs.
// - Ferma lo sniffer e ripristina la STA usando le credenziali salvate.
// - Alla fine invoca mqttReconnect callback se registrata.
//
// ATTENZIONE: Questa funzione effettua operazioni di WiFi (stop/init) e può bloccare:
// chiamala da un task separato se non vuoi bloccare il loop principale.
void doFullSweepAndReconnect(unsigned long sweepMs);

// Trova indice device (thread-safe se chiami con devicesMux o usi findDeviceIndexLocked in impl).
int findDeviceIndex(const uint8_t *mac);

