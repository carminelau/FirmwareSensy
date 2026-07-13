#pragma once

#include <stddef.h>
#include <stdint.h>

#ifndef FW_LOG_LEVEL
#define FW_LOG_LEVEL 1
#endif

constexpr int FW_LOG_MINIMAL = 1;
constexpr int FW_LOG_FULL = 3;
static_assert(FW_LOG_LEVEL == FW_LOG_MINIMAL || FW_LOG_LEVEL == FW_LOG_FULL,
              "FW_LOG_LEVEL must be 1 (MINIMAL) or 3 (FULL)");

#ifndef FW_LOG_BILINGUAL
#define FW_LOG_BILINGUAL 0
#endif

#ifndef ENABLE_MICS4514
#define ENABLE_MICS4514 0
#endif

namespace RUNTIME_CONTRACT
{
constexpr bool SNIFFER_ALWAYS_ON = true;
}

namespace EEPROM_ADDR
{
constexpr int SSID_OFFSET = 0;
constexpr int PASSWORD_OFFSET = 32;
constexpr int WIFI_FLAG_ADDR = 97;
constexpr int CONF_FLAG_ADDR = 101;
constexpr int VERSION_OFFSET = 104;
constexpr int TOPIC_OFFSET = 126;
constexpr size_t VERSION_MAX_LENGTH = TOPIC_OFFSET - VERSION_OFFSET - 1;
constexpr int LOW_POWER_FLAG_ADDR = 205;
constexpr int SNIFFER_FLAG_ADDR = 206;
constexpr int RELAY1_STATE_ADDR = 207;
constexpr int RELAY2_STATE_ADDR = 208;
constexpr int EEPROM_INIT_FLAG_ADDR = 511;
constexpr uint8_t EEPROM_INIT_MAGIC = 0xAA;
constexpr size_t EEPROM_SIZE = 512;
}

namespace SERIAL_CONFIG
{
constexpr unsigned long BAUD_RATE = 115200;
constexpr unsigned long DELAY_SHORT = 500;
constexpr unsigned long DELAY_MID = 1000;
constexpr unsigned long DELAY_LONG = 12000;
}

namespace WIFI_CONFIG
{
constexpr int MAX_SSID_LENGTH = 32;
constexpr int MAX_PASSWORD_LENGTH = 64;
constexpr int MAX_CONNECT_ATTEMPTS = 6;
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 1000;
}

namespace SENSOR_DELAYS
{
constexpr unsigned long SENSOR_INIT_DELAY = 10000;
constexpr unsigned long SENSOR_READ_DELAY = 1000;
}

namespace LED_COLORS
{
struct RGB
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

constexpr RGB BLUE = {0, 0, 255};
constexpr RGB GREEN = {0, 255, 0};
constexpr RGB PURPLE = {255, 0, 255};
constexpr RGB RED = {255, 0, 0};
constexpr RGB YELLOW = {255, 255, 0};
constexpr RGB CYAN = {0, 255, 255};
constexpr RGB WHITE = {255, 255, 255};
constexpr RGB BLACK = {0, 0, 0};
constexpr uint8_t BLUE_R = 0, BLUE_G = 0, BLUE_B = 255;
constexpr uint8_t GREEN_R = 0, GREEN_G = 255, GREEN_B = 0;
constexpr uint8_t PURPLE_R = 255, PURPLE_G = 0, PURPLE_B = 255;
}

static_assert(EEPROM_ADDR::EEPROM_INIT_FLAG_ADDR < EEPROM_ADDR::EEPROM_SIZE,
              "EEPROM init flag outside storage");
static_assert(EEPROM_ADDR::VERSION_MAX_LENGTH == 21,
              "Firmware name storage contract changed");
