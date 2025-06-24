/**********************************
 *        LIBRERIE INCLUSE        *
 **********************************/
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <time.h>
#include <TimeLib.h>

// Librerie sensori
#include "SHT21.h"
#include <Multichannel_Gas_GMXXX.h>
#include "DFRobot_OzoneSensor.h"
#include "PMserial.h"
#include "DFRobot_MICS.h"
#include <sps30.h>
#include "SensirionI2CSen5x.h"
#include <SensirionI2cScd30.h>
#include <SensirionI2cScd4x.h>
#include <BH1750.h>
#include <ModbusSensorLibrary.h>

// Libreria GPS
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>

// Librerie WiFi e MQTT
#include "WiFi.h"
#include <WiFiClient.h>
#include <MQTT.h>
#include <ArduinoHttpClient.h>
#include "HTTPClient.h"

// Web Server
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// OTA e aggiornamenti
#include <Update.h>

// File system
#include "FS.h"
#include "SPIFFS.h"
#include "SD.h"
#include "SPI.h"

// Libreria JSON
#include "ArduinoJson.h"

// Libreria personalizzata
//#include "hywdc6e-lib.h"

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
int dir = 0;
int count = 0;
bool arrived = false;
bool daresettare = false;
bool sd = false;
bool connected = false;
bool conf = false;
bool AP = false;
bool wifi = false;

char myConcatenation[12];

String topic = "";
String nameBinESP = "";
String nameBinServer;
String topicListen;
String timezone_it = "CET-1CEST,M3.5.0,M10.5.0/3";
const char *verionBoard = STR(YEAR) "V" STR(VERSION);

/**********************************
 *     VARIABILI DI STATO         *
 **********************************/
bool sps, ozone, pmsa003, gas, sht, ane, low, lux, soil;
bool sen55, scd30, scd41, mics4514;
bool accesspoint, relay1, relay2;

int i2cControl = 0;
int GasCicle = 0;
int saveCounterSD = 0;
int saveCounterSPIFFS = 0;
int epochs = 0;
int tmpRelay, tmpRelay1, tmpPausa, tmpTotale;
int resetCount = 0;

unsigned long timeNow = 0;
unsigned long timeLast = 0;
unsigned long diffe = 0;

volatile int state = LOW;
volatile int state_short = LOW;
volatile int state_long = LOW;
volatile unsigned long current_high;
volatile unsigned long current_low;

/**********************************
 *     OGGETTI SENSORI E HW       *
 **********************************/
AsyncWebServer server(80);
TaskHandle_t Task1;
TaskHandle_t Task2;
BH1750 lightMeter;

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
int GPSBaud = 9600;
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

// Multigas
float no2, c2h5oh, voc, co, no2_index;
float nh3;

// PMS/SPS
float pmAe1_0, pmAe2_5, pmAe10_0;
float pmAe1_0Test, pmAe2_5Test, pmAe10_0Test;

// SEN55
float sen55_temp = 0.0;
float sen55_hum = 0.0;

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

// Funzioni generiche
int dir_wind_fix(int dire);
float round_float(float value);

// EEPROM
void write_inside_eeprom(bool val, int addr);
void read_topic_eeprom();
void read_version_eeprom();
void write_conf_eeprom(bool b);
bool read_conf_eeprom();
bool read_wifi_eeprom();
void write_string_eeprom(char *c, int offset);
void write_string_eeprom(String c, int offset);
void write_low_eeprom(bool val);
bool read_low_eeprom();
void check_vergin_eeprom();
void delete_info_sensy();
void delete_wifi_settings();

// I2C e periferiche
void init_i2c();

// Sensori multigas
bool init_multigas();
void read_multigas();
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

void check_sensors_diagnostics();
void send_sensors_diagnostics();

// GPS
bool init_gps();

// RTC e tempo
void init_rtc();
int get_epoch();
int get_epoch_storage();
unsigned long get_epoch_ntp_server();
void set_rtc(int timestamp);
void set_timezone(String timezone);

// WiFi e Access Point
bool connect_wifi_network();
void init_wifi();
void create_access_point();
void disconnect_access_point();
void get_mac_address();
String get_id_square();

// MQTT
void init_mqtt();
void connect_mqtt_client();
void loop_mqtt();
bool send_data_mqtt();
void read_message_received_mqtt(String &topic, String &payload);
void delete_message_received_mqtt();
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

// Anemometro
void read_anemometer();

// Soil Moisture
bool check_soil_moisture();
void read_soil_moisture();

// Rele
bool init_relay(int relayPin);

// Task paralleli
void loop_1_core(void *pvParameters);
void loop_0_core(void *pvParameters);

// Utility
bool find_arduino_devices();

bool get_nearest_data(const String &params);
void parse_response(const String &payload);
void process_token(const String &token);
String vector_to_encoded_json_array(const std::vector<String> &vec);
String get_list_wifi();