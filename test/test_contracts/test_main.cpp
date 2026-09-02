#include <unity.h>

#include "app_config.h"
#include "pollutant_mask.h"
#include "protocol_contracts.h"
#include "runtime_model.h"

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
    TEST_ASSERT_FALSE(pollutants.add("pm1"));
    TEST_ASSERT_FALSE(pollutants.add("unknown"));
    TEST_ASSERT_EQUAL_UINT32(2, pollutants.size());
    TEST_ASSERT_EQUAL_STRING("pm1", pollutants[0]);
    TEST_ASSERT_EQUAL_STRING("co2", pollutants[1]);
    TEST_ASSERT_TRUE(pollutants.contains("pm1"));
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
    TEST_ASSERT_EQUAL_STRING("/get_runtime_model", HTTP_CONTRACT::RUNTIME_MODEL_ROUTE);
    TEST_ASSERT_EQUAL_STRING("/get_runtime_models", HTTP_CONTRACT::RUNTIME_MODELS_ROUTE);
    TEST_ASSERT_EQUAL_UINT32(5, HTTP_CONTRACT::DIAGNOSTICS_QUERY_FIELD_COUNT);
    TEST_ASSERT_EQUAL_STRING("sensors", HTTP_CONTRACT::DIAGNOSTICS_QUERY_FIELDS[0]);
    TEST_ASSERT_EQUAL_STRING("ID", HTTP_CONTRACT::DIAGNOSTICS_QUERY_FIELDS[1]);
    TEST_ASSERT_EQUAL_STRING("versione", HTTP_CONTRACT::DIAGNOSTICS_QUERY_FIELDS[2]);
    TEST_ASSERT_EQUAL_STRING("board", HTTP_CONTRACT::DIAGNOSTICS_QUERY_FIELDS[3]);
    TEST_ASSERT_EQUAL_STRING("info", HTTP_CONTRACT::DIAGNOSTICS_QUERY_FIELDS[4]);
}

static RuntimeModel example_runtime_model()
{
    RuntimeModel model = runtime_model_defaults();
    model.enabled = true;
    model.elapsedDaysOriginEpoch = 1704067200UL;
    model.no2RawScale = 0.01f;
    model.vocRawScale = 0.01f;
    model.coefficients = {
        89.84894046f,
        3.08967148f,
        0.04722309364f,
        0.2073998517f,
        -0.00771123405f,
        0.732155505f,
        0.4504224958f,
        1.521400947f,
        -0.230145137f,
        0.03411805408f,
        -0.05224941417f,
        -0.09159412337f,
        -0.6606045594f,
        0.06157221149f,
        -0.02769616655f,
        -0.0003588672809f,
        0.00106320889f,
        -0.0003769268059f,
        -2.511363471f};
    return model;
}

static void test_runtime_model_equation()
{
    RuntimeModel model = example_runtime_model();
    RuntimeModelFeatures features = {
        3.2f, 40.0f, 35.0f, 60.0f,
        3.0f, 2.8f, 3.1f,
        38.0f, 36.0f, 39.0f,
        120.0f};

    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 78.315535f,
                             evaluate_runtime_model(model, features));
}

static void test_runtime_history_builds_lags_and_rolling_window()
{
    RuntimeModel model = example_runtime_model();
    RuntimeFeatureHistory history = {};
    runtime_history_reset(history);

    const uint32_t currentEpoch = model.elapsedDaysOriginEpoch + 4UL * 3600UL;
    const uint32_t windowStart = currentEpoch - model.rollingWindowSeconds;
    for (uint32_t epoch = windowStart; epoch <= currentEpoch; epoch += 300UL)
    {
        TEST_ASSERT_TRUE(runtime_history_add(history, epoch, 100.0f, 200.0f, 300UL));
    }

    RuntimeModelFeatures features = {};
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RuntimeFeatureStatus::READY),
        static_cast<int>(build_runtime_model_features(model, history, currentEpoch,
                                                      100.0f, 200.0f, 25.0f, 50.0f,
                                                      features)));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, features.no2Raw);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.0f, features.vocRaw);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, features.no2RawLag1);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, features.no2RawLag2);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, features.no2RawRolling);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.0f, features.vocRawRolling);
}

static void test_runtime_history_rejects_incomplete_lags()
{
    RuntimeModel model = example_runtime_model();
    RuntimeFeatureHistory history = {};
    runtime_history_reset(history);
    const uint32_t currentEpoch = model.elapsedDaysOriginEpoch + 4UL * 3600UL;
    TEST_ASSERT_TRUE(runtime_history_add(history, currentEpoch, 100.0f, 200.0f, 300UL));

    RuntimeModelFeatures features = {};
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RuntimeFeatureStatus::LAG_1_MISSING),
        static_cast<int>(build_runtime_model_features(model, history, currentEpoch,
                                                      100.0f, 200.0f, 25.0f, 50.0f,
                                                      features)));
}

static void test_runtime_model_registry_keeps_one_slot_per_pollutant()
{
    RuntimeModelRegistry registry = {};
    runtime_model_registry_reset(registry);

    const int no2Index = runtime_model_registry_add(registry, "no2");
    const int coIndex = runtime_model_registry_add(registry, "co");
    const int duplicateNo2Index = runtime_model_registry_add(registry, "no2");

    TEST_ASSERT_EQUAL_INT(0, no2Index);
    TEST_ASSERT_EQUAL_INT(1, coIndex);
    TEST_ASSERT_EQUAL_INT(no2Index, duplicateNo2Index);
    TEST_ASSERT_EQUAL_UINT32(2, registry.count);
    TEST_ASSERT_EQUAL_STRING("no2", registry.slots[no2Index].pollutant);
    TEST_ASSERT_EQUAL_STRING("co", registry.slots[coIndex].pollutant);
    TEST_ASSERT_EQUAL_INT(coIndex, runtime_model_registry_find(registry, "co"));
    TEST_ASSERT_EQUAL_INT(-1, runtime_model_registry_find(registry, "o3"));
}

static void test_runtime_history_merge_survives_out_of_order_bootstrap()
{
    RuntimeFeatureHistory history = {};
    runtime_history_reset(history);

    TEST_ASSERT_TRUE(runtime_history_merge_sample(history, 3000UL, 30.0f, 300.0f));
    TEST_ASSERT_TRUE(runtime_history_merge_sample(history, 1000UL, 10.0f, 100.0f));
    TEST_ASSERT_TRUE(runtime_history_merge_sample(history, 2000UL, 20.0f, 200.0f));
    TEST_ASSERT_EQUAL_UINT16(3, history.count);

    RuntimeHistorySample nearest = {};
    TEST_ASSERT_TRUE(runtime_history_nearest(history, 2000UL, 0UL, nearest));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 20.0f, nearest.no2HardwareRaw);
    TEST_ASSERT_TRUE(runtime_history_merge_sample(history, 2000UL, 21.0f, 201.0f));
    TEST_ASSERT_EQUAL_UINT16(3, history.count);
    TEST_ASSERT_TRUE(runtime_history_nearest(history, 2000UL, 0UL, nearest));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 21.0f, nearest.no2HardwareRaw);
    TEST_ASSERT_NOT_EQUAL(0UL, runtime_history_checksum(history));
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
    RUN_TEST(test_runtime_model_equation);
    RUN_TEST(test_runtime_history_builds_lags_and_rolling_window);
    RUN_TEST(test_runtime_history_rejects_incomplete_lags);
    RUN_TEST(test_runtime_model_registry_keeps_one_slot_per_pollutant);
    RUN_TEST(test_runtime_history_merge_survives_out_of_order_bootstrap);
    return UNITY_END();
}
