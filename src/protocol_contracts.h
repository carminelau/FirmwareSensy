#pragma once

#include <stddef.h>
#include <stdint.h>
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
    RELAY1_ON_TIMED,
    RELAY2_ON_TIMED,
    BOTH_ON_TIMED,
    LOW_POWER_ON,
    LOW_POWER_OFF,
    RESET
};

struct ParsedMqttCommand
{
    MqttCommand command;
    uint16_t durationSeconds;
};

inline ParsedMqttCommand parse_mqtt_command_with_duration(const char *payload)
{
    if (payload == nullptr)
    {
        return {MqttCommand::NONE, 0};
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
            return {entries[i].command, 0};
        }
    }

    struct TimedEntry
    {
        const char *prefix;
        MqttCommand command;
    };

    static const TimedEntry timedEntries[] = {
        {"on:", MqttCommand::RELAY1_ON_TIMED},
        {"on2:", MqttCommand::RELAY2_ON_TIMED},
        {"onon:", MqttCommand::BOTH_ON_TIMED},
    };

    for (size_t i = 0; i < sizeof(timedEntries) / sizeof(timedEntries[0]); ++i)
    {
        const size_t prefixLength = strlen(timedEntries[i].prefix);
        if (strncmp(payload, timedEntries[i].prefix, prefixLength) != 0)
        {
            continue;
        }

        const char *secondsText = payload + prefixLength;
        const size_t digitCount = strlen(secondsText);
        if (digitCount == 0 || digitCount > 4)
        {
            return {MqttCommand::NONE, 0};
        }

        uint16_t seconds = 0;
        for (size_t digit = 0; digit < digitCount; ++digit)
        {
            if (secondsText[digit] < '0' || secondsText[digit] > '9')
            {
                return {MqttCommand::NONE, 0};
            }
            seconds = static_cast<uint16_t>(seconds * 10U +
                                            static_cast<uint16_t>(secondsText[digit] - '0'));
        }

        if (seconds == 0 || seconds > 9999)
        {
            return {MqttCommand::NONE, 0};
        }
        return {timedEntries[i].command, seconds};
    }

    return {MqttCommand::NONE, 0};
}

inline MqttCommand parse_mqtt_command(const char *payload)
{
    return parse_mqtt_command_with_duration(payload).command;
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
