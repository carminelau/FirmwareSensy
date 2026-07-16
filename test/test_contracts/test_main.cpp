#include <unity.h>

#include "app_config.h"
#include "pollutant_mask.h"
#include "protocol_contracts.h"

void setUp() {}
void tearDown() {}

static void test_eeprom_contract()
{
    TEST_ASSERT_EQUAL_INT(0, EEPROM_ADDR::SSID_OFFSET);
    TEST_ASSERT_EQUAL_INT(32, EEPROM_ADDR::PASSWORD_OFFSET);
    TEST_ASSERT_EQUAL_INT(97, EEPROM_ADDR::WIFI_FLAG_ADDR);
    TEST_ASSERT_EQUAL_INT(101, EEPROM_ADDR::CONF_FLAG_ADDR);
    TEST_ASSERT_EQUAL_INT(104, EEPROM_ADDR::VERSION_OFFSET);
    TEST_ASSERT_EQUAL_INT(126, EEPROM_ADDR::TOPIC_OFFSET);
    TEST_ASSERT_EQUAL_INT(205, EEPROM_ADDR::LOW_POWER_FLAG_ADDR);
    TEST_ASSERT_EQUAL_INT(206, EEPROM_ADDR::SNIFFER_FLAG_ADDR);
    TEST_ASSERT_EQUAL_INT(207, EEPROM_ADDR::RELAY1_STATE_ADDR);
    TEST_ASSERT_EQUAL_INT(208, EEPROM_ADDR::RELAY2_STATE_ADDR);
    TEST_ASSERT_EQUAL_HEX8(0xAA, EEPROM_ADDR::EEPROM_INIT_MAGIC);
}

static void test_mqtt_command_contract()
{
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MqttCommand::RELAY1_ON), static_cast<int>(parse_mqtt_command("on")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MqttCommand::RELAY1_OFF), static_cast<int>(parse_mqtt_command("of")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MqttCommand::RELAY2_ON), static_cast<int>(parse_mqtt_command("on2")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MqttCommand::RELAY2_OFF), static_cast<int>(parse_mqtt_command("of2")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MqttCommand::BOTH_ON), static_cast<int>(parse_mqtt_command("onon")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MqttCommand::BOTH_OFF), static_cast<int>(parse_mqtt_command("ofof")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MqttCommand::RELAY1_ON_RELAY2_OFF), static_cast<int>(parse_mqtt_command("onof")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MqttCommand::RELAY1_OFF_RELAY2_ON), static_cast<int>(parse_mqtt_command("ofon")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MqttCommand::LOW_POWER_ON), static_cast<int>(parse_mqtt_command("low1")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MqttCommand::LOW_POWER_OFF), static_cast<int>(parse_mqtt_command("low0")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MqttCommand::RESET), static_cast<int>(parse_mqtt_command("reset")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MqttCommand::NONE), static_cast<int>(parse_mqtt_command("ON")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MqttCommand::NONE), static_cast<int>(parse_mqtt_command("low2")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MqttCommand::NONE), static_cast<int>(parse_mqtt_command(nullptr)));
}

static void test_pollutants_are_unique_and_ordered()
{
    PollutantMask pollutants;
    TEST_ASSERT_TRUE(pollutants.add("pm1"));
    TEST_ASSERT_TRUE(pollutants.add("co2"));
    TEST_ASSERT_TRUE(pollutants.add("Multi_no2"));
    TEST_ASSERT_TRUE(pollutants.add("HD_no2"));
    TEST_ASSERT_TRUE(pollutants.add("old_o3"));
    TEST_ASSERT_FALSE(pollutants.add("pm1"));
    TEST_ASSERT_FALSE(pollutants.add("unknown"));
    TEST_ASSERT_EQUAL_UINT32(5, pollutants.size());
    TEST_ASSERT_EQUAL_STRING("pm1", pollutants[0]);
    TEST_ASSERT_EQUAL_STRING("co2", pollutants[1]);
    TEST_ASSERT_EQUAL_STRING("Multi_no2", pollutants[2]);
    TEST_ASSERT_EQUAL_STRING("HD_no2", pollutants[3]);
    TEST_ASSERT_EQUAL_STRING("old_o3", pollutants[4]);
    TEST_ASSERT_TRUE(pollutants.contains("pm1"));
    TEST_ASSERT_TRUE(pollutants.contains("Multi_no2"));
    TEST_ASSERT_TRUE(pollutants.contains("HD_no2"));
    TEST_ASSERT_TRUE(pollutants.contains("old_o3"));
    pollutants.clear();
    TEST_ASSERT_EQUAL_UINT32(0, pollutants.size());
}

static void test_sniffer_adjustment_contract()
{
    TEST_ASSERT_TRUE(RUNTIME_CONTRACT::SNIFFER_ALWAYS_ON);
    TEST_ASSERT_EQUAL_INT(130, adjusted_mobile_device_count(120, 20));
    TEST_ASSERT_EQUAL_INT(0, adjusted_mobile_device_count(10, 10));
    TEST_ASSERT_EQUAL_INT(0, adjusted_mobile_device_count(5, 10));
}

static void test_diagnostics_http_contract()
{
    TEST_ASSERT_EQUAL_STRING("/set_sensors", HTTP_CONTRACT::DIAGNOSTICS_ROUTE);
    TEST_ASSERT_EQUAL_UINT32(5, HTTP_CONTRACT::DIAGNOSTICS_QUERY_FIELD_COUNT);
    TEST_ASSERT_EQUAL_STRING("sensors", HTTP_CONTRACT::DIAGNOSTICS_QUERY_FIELDS[0]);
    TEST_ASSERT_EQUAL_STRING("ID", HTTP_CONTRACT::DIAGNOSTICS_QUERY_FIELDS[1]);
    TEST_ASSERT_EQUAL_STRING("versione", HTTP_CONTRACT::DIAGNOSTICS_QUERY_FIELDS[2]);
    TEST_ASSERT_EQUAL_STRING("board", HTTP_CONTRACT::DIAGNOSTICS_QUERY_FIELDS[3]);
    TEST_ASSERT_EQUAL_STRING("info", HTTP_CONTRACT::DIAGNOSTICS_QUERY_FIELDS[4]);
}

static void test_firmware_name_storage_contract()
{
    TEST_ASSERT_EQUAL_UINT32(21, EEPROM_ADDR::VERSION_MAX_LENGTH);
    TEST_ASSERT_TRUE(firmware_name_fits_storage("FW_ST_2024V4_V3.bin"));
    TEST_ASSERT_TRUE(firmware_name_fits_storage("123456789012345678901"));
    TEST_ASSERT_FALSE(firmware_name_fits_storage(""));
    TEST_ASSERT_FALSE(firmware_name_fits_storage("1234567890123456789012"));
    TEST_ASSERT_FALSE(firmware_name_fits_storage(nullptr));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_eeprom_contract);
    RUN_TEST(test_mqtt_command_contract);
    RUN_TEST(test_pollutants_are_unique_and_ordered);
    RUN_TEST(test_sniffer_adjustment_contract);
    RUN_TEST(test_diagnostics_http_contract);
    RUN_TEST(test_firmware_name_storage_contract);
    return UNITY_END();
}
