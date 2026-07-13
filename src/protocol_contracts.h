#pragma once

#include <stddef.h>
#include <string.h>

#include "app_config.h"

namespace HTTP_CONTRACT
{
constexpr const char *DIAGNOSTICS_ROUTE = "/set_sensors";
constexpr const char *DIAGNOSTICS_QUERY_FIELDS[] = {"sensors", "ID", "versione", "board", "info"};
constexpr size_t DIAGNOSTICS_QUERY_FIELD_COUNT =
    sizeof(DIAGNOSTICS_QUERY_FIELDS) / sizeof(DIAGNOSTICS_QUERY_FIELDS[0]);
}

enum class MqttCommand : unsigned char
{
    NONE,
    RELAY1_ON,
    RELAY1_OFF,
    RELAY2_ON,
    RELAY2_OFF,
    BOTH_ON,
    BOTH_OFF,
    RELAY1_ON_RELAY2_OFF,
    RELAY1_OFF_RELAY2_ON,
    LOW_POWER_ON,
    LOW_POWER_OFF,
    RESET
};

inline MqttCommand parse_mqtt_command(const char *payload)
{
    if (payload == nullptr)
    {
        return MqttCommand::NONE;
    }

    struct Entry
    {
        const char *payload;
        MqttCommand command;
    };

    static const Entry entries[] = {
        {"on", MqttCommand::RELAY1_ON},
        {"of", MqttCommand::RELAY1_OFF},
        {"on2", MqttCommand::RELAY2_ON},
        {"of2", MqttCommand::RELAY2_OFF},
        {"onon", MqttCommand::BOTH_ON},
        {"ofof", MqttCommand::BOTH_OFF},
        {"onof", MqttCommand::RELAY1_ON_RELAY2_OFF},
        {"ofon", MqttCommand::RELAY1_OFF_RELAY2_ON},
        {"low1", MqttCommand::LOW_POWER_ON},
        {"low0", MqttCommand::LOW_POWER_OFF},
        {"reset", MqttCommand::RESET},
    };

    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); ++i)
    {
        if (strcmp(payload, entries[i].payload) == 0)
        {
            return entries[i].command;
        }
    }
    return MqttCommand::NONE;
}

inline int adjusted_mobile_device_count(int registered, int fixed, float multiplier = 1.30f)
{
    const int mobile = registered > fixed ? registered - fixed : 0;
    return static_cast<int>(mobile * multiplier);
}

inline bool firmware_name_fits_storage(const char *name)
{
    if (name == nullptr)
        return false;
    const size_t length = strlen(name);
    return length > 0 && length <= EEPROM_ADDR::VERSION_MAX_LENGTH;
}
