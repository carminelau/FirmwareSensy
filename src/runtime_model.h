#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace RUNTIME_MODEL_CONTRACT
{
constexpr uint16_t SCHEMA_VERSION = 1;
constexpr uint32_t DEFAULT_REFRESH_SECONDS = 21600UL;
constexpr uint32_t MIN_REFRESH_SECONDS = 300UL;
constexpr uint32_t MAX_REFRESH_SECONDS = 604800UL;
constexpr size_t MODEL_ID_MAX_LENGTH = 47;
constexpr size_t OUTPUT_FIELD_MAX_LENGTH = 31;
constexpr size_t OUTPUT_UNIT_MAX_LENGTH = 15;
constexpr size_t POLLUTANT_MAX_LENGTH = 15;
constexpr size_t MAX_MODELS = 16;
constexpr size_t HISTORY_CAPACITY = 64;
constexpr uint32_t HISTORY_MAGIC = 0x534D4F44UL; // "SMOD"
constexpr uint16_t HISTORY_VERSION = 1;
constexpr uint32_t HISTORY_STORAGE_MAGIC = 0x53485354UL; // "SHST"
constexpr uint16_t HISTORY_STORAGE_VERSION = 1;
}

struct RuntimeModelCoefficients
{
    float intercept;
    float no2Raw;
    float vocRaw;
    float temperature;
    float humidity;
    float no2RawLag1;
    float no2RawLag2;
    float no2RawRolling;
    float vocRawLag1;
    float vocRawLag2;
    float vocRawRolling;
    float elapsedDays;
    float no2RawSquared;
    float no2RawTemperature;
    float no2RawHumidity;
    float vocRawSquared;
    float vocRawTemperature;
    float vocRawHumidity;
    float temperatureAboveThreshold;
};

struct RuntimeModel
{
    uint16_t schemaVersion;
    bool enabled;
    char modelId[RUNTIME_MODEL_CONTRACT::MODEL_ID_MAX_LENGTH + 1];
    char outputField[RUNTIME_MODEL_CONTRACT::OUTPUT_FIELD_MAX_LENGTH + 1];
    char outputUnit[RUNTIME_MODEL_CONTRACT::OUTPUT_UNIT_MAX_LENGTH + 1];
    float outputClampMin;
    float outputClampMax;
    bool clampMinEnabled;
    bool clampMaxEnabled;
    float no2RawScale;
    float no2RawOffset;
    float vocRawScale;
    float vocRawOffset;
    uint32_t elapsedDaysOriginEpoch;
    uint32_t historySamplePeriodSeconds;
    uint32_t lag1Seconds;
    uint32_t lag2Seconds;
    uint32_t rollingWindowSeconds;
    uint32_t lagToleranceSeconds;
    uint16_t minimumRollingSamples;
    bool requireFullRollingWindow;
    float temperatureThresholdC;
    uint32_t refreshAfterSeconds;
    RuntimeModelCoefficients coefficients;
};

struct RuntimeHistorySample
{
    uint32_t epoch;
    float no2HardwareRaw;
    float vocHardwareRaw;
};

struct RuntimeFeatureHistory
{
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    uint16_t nextIndex;
    uint16_t reserved;
    RuntimeHistorySample samples[RUNTIME_MODEL_CONTRACT::HISTORY_CAPACITY];
};

struct RuntimeModelFeatures
{
    float no2Raw;
    float vocRaw;
    float temperature;
    float humidity;
    float no2RawLag1;
    float no2RawLag2;
    float no2RawRolling;
    float vocRawLag1;
    float vocRawLag2;
    float vocRawRolling;
    float elapsedDays;
};

enum class RuntimeFeatureStatus : uint8_t
{
    READY,
    MODEL_DISABLED,
    INVALID_CURRENT_INPUT,
    INVALID_TIME,
    LAG_1_MISSING,
    LAG_2_MISSING,
    ROLLING_WINDOW_INCOMPLETE
};

struct RuntimeModelSlot
{
    char pollutant[RUNTIME_MODEL_CONTRACT::POLLUTANT_MAX_LENGTH + 1];
    RuntimeModel model;
    bool loaded;
    bool storageChecked;
    uint32_t checkedAtEpoch;
    unsigned long lastFetchAttemptMs;
    RuntimeFeatureStatus lastFeatureStatus;
};

struct RuntimeModelRegistry
{
    size_t count;
    RuntimeModelSlot slots[RUNTIME_MODEL_CONTRACT::MAX_MODELS];
};

inline RuntimeModel runtime_model_defaults()
{
    RuntimeModel model = {};
    model.schemaVersion = RUNTIME_MODEL_CONTRACT::SCHEMA_VERSION;
    model.enabled = false;
    model.no2RawScale = 1.0f;
    model.vocRawScale = 1.0f;
    model.historySamplePeriodSeconds = 300UL;
    model.lag1Seconds = 3600UL;
    model.lag2Seconds = 7200UL;
    model.rollingWindowSeconds = 10800UL;
    model.lagToleranceSeconds = 450UL;
    model.minimumRollingSamples = 12;
    model.requireFullRollingWindow = true;
    model.temperatureThresholdC = 30.0f;
    model.refreshAfterSeconds = RUNTIME_MODEL_CONTRACT::DEFAULT_REFRESH_SECONDS;
    return model;
}

inline void runtime_model_registry_reset(RuntimeModelRegistry &registry)
{
    memset(&registry, 0, sizeof(registry));
}

inline int runtime_model_registry_find(const RuntimeModelRegistry &registry,
                                       const char *pollutant)
{
    if (pollutant == nullptr || pollutant[0] == '\0')
    {
        return -1;
    }
    for (size_t i = 0; i < registry.count; ++i)
    {
        if (strcmp(registry.slots[i].pollutant, pollutant) == 0)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

inline int runtime_model_registry_add(RuntimeModelRegistry &registry,
                                      const char *pollutant)
{
    const int existing = runtime_model_registry_find(registry, pollutant);
    if (existing >= 0)
    {
        return existing;
    }
    if (pollutant == nullptr || pollutant[0] == '\0' ||
        strlen(pollutant) > RUNTIME_MODEL_CONTRACT::POLLUTANT_MAX_LENGTH ||
        registry.count >= RUNTIME_MODEL_CONTRACT::MAX_MODELS)
    {
        return -1;
    }

    RuntimeModelSlot &slot = registry.slots[registry.count];
    memset(&slot, 0, sizeof(slot));
    strncpy(slot.pollutant, pollutant, sizeof(slot.pollutant) - 1U);
    slot.model = runtime_model_defaults();
    slot.lastFeatureStatus = RuntimeFeatureStatus::READY;
    return static_cast<int>(registry.count++);
}

inline void runtime_history_reset(RuntimeFeatureHistory &history)
{
    memset(&history, 0, sizeof(history));
    history.magic = RUNTIME_MODEL_CONTRACT::HISTORY_MAGIC;
    history.version = RUNTIME_MODEL_CONTRACT::HISTORY_VERSION;
}

inline bool runtime_history_valid(const RuntimeFeatureHistory &history)
{
    return history.magic == RUNTIME_MODEL_CONTRACT::HISTORY_MAGIC &&
           history.version == RUNTIME_MODEL_CONTRACT::HISTORY_VERSION &&
           history.count <= RUNTIME_MODEL_CONTRACT::HISTORY_CAPACITY &&
           history.nextIndex < RUNTIME_MODEL_CONTRACT::HISTORY_CAPACITY;
}

inline void runtime_history_ensure_valid(RuntimeFeatureHistory &history)
{
    if (!runtime_history_valid(history))
    {
        runtime_history_reset(history);
    }
}

inline int runtime_history_latest_index(const RuntimeFeatureHistory &history)
{
    if (!runtime_history_valid(history) || history.count == 0)
    {
        return -1;
    }

    uint32_t latestEpoch = 0;
    int latestIndex = -1;
    for (size_t i = 0; i < history.count; ++i)
    {
        if (history.samples[i].epoch >= latestEpoch)
        {
            latestEpoch = history.samples[i].epoch;
            latestIndex = static_cast<int>(i);
        }
    }
    return latestIndex;
}

inline bool runtime_history_add(RuntimeFeatureHistory &history, uint32_t epoch,
                                float no2HardwareRaw, float vocHardwareRaw,
                                uint32_t minimumPeriodSeconds)
{
    runtime_history_ensure_valid(history);
    if (epoch == 0 || !isfinite(no2HardwareRaw) || !isfinite(vocHardwareRaw))
    {
        return false;
    }

    const int latestIndex = runtime_history_latest_index(history);
    if (latestIndex >= 0)
    {
        const uint32_t latestEpoch = history.samples[latestIndex].epoch;
        if (epoch < latestEpoch)
        {
            return false;
        }
        if (epoch == latestEpoch)
        {
            history.samples[latestIndex].no2HardwareRaw = no2HardwareRaw;
            history.samples[latestIndex].vocHardwareRaw = vocHardwareRaw;
            return true;
        }
        if (epoch - latestEpoch < minimumPeriodSeconds)
        {
            return false;
        }
    }

    history.samples[history.nextIndex] = {epoch, no2HardwareRaw, vocHardwareRaw};
    history.nextIndex = static_cast<uint16_t>((history.nextIndex + 1U) %
                                              RUNTIME_MODEL_CONTRACT::HISTORY_CAPACITY);
    if (history.count < RUNTIME_MODEL_CONTRACT::HISTORY_CAPACITY)
    {
        ++history.count;
    }
    return true;
}

inline bool runtime_history_merge_sample(RuntimeFeatureHistory &history, uint32_t epoch,
                                         float no2HardwareRaw, float vocHardwareRaw)
{
    runtime_history_ensure_valid(history);
    if (epoch == 0 || !isfinite(no2HardwareRaw) || !isfinite(vocHardwareRaw))
    {
        return false;
    }

    for (size_t i = 0; i < history.count; ++i)
    {
        if (history.samples[i].epoch == epoch)
        {
            history.samples[i].no2HardwareRaw = no2HardwareRaw;
            history.samples[i].vocHardwareRaw = vocHardwareRaw;
            return true;
        }
    }

    if (history.count < RUNTIME_MODEL_CONTRACT::HISTORY_CAPACITY)
    {
        history.samples[history.count++] = {epoch, no2HardwareRaw, vocHardwareRaw};
        history.nextIndex = static_cast<uint16_t>(history.count %
                                                  RUNTIME_MODEL_CONTRACT::HISTORY_CAPACITY);
        return true;
    }

    size_t oldestIndex = 0;
    for (size_t i = 1; i < history.count; ++i)
    {
        if (history.samples[i].epoch < history.samples[oldestIndex].epoch)
        {
            oldestIndex = i;
        }
    }
    if (epoch < history.samples[oldestIndex].epoch)
    {
        return false;
    }

    history.samples[oldestIndex] = {epoch, no2HardwareRaw, vocHardwareRaw};
    history.nextIndex = static_cast<uint16_t>((oldestIndex + 1U) %
                                              RUNTIME_MODEL_CONTRACT::HISTORY_CAPACITY);
    return true;
}

inline uint32_t runtime_history_checksum(const RuntimeFeatureHistory &history)
{
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&history);
    uint32_t hash = 2166136261UL;
    for (size_t i = 0; i < sizeof(history); ++i)
    {
        hash ^= bytes[i];
        hash *= 16777619UL;
    }
    return hash;
}

inline float runtime_model_scale_raw(float hardwareRaw, float scale, float offset)
{
    return hardwareRaw * scale + offset;
}

inline bool runtime_history_nearest(const RuntimeFeatureHistory &history, uint32_t targetEpoch,
                                    uint32_t toleranceSeconds, RuntimeHistorySample &result)
{
    if (!runtime_history_valid(history) || history.count == 0)
    {
        return false;
    }

    uint32_t bestDistance = UINT32_MAX;
    bool found = false;
    for (size_t i = 0; i < history.count; ++i)
    {
        const RuntimeHistorySample &sample = history.samples[i];
        const uint32_t distance = sample.epoch > targetEpoch
                                      ? sample.epoch - targetEpoch
                                      : targetEpoch - sample.epoch;
        if (distance <= toleranceSeconds && distance < bestDistance)
        {
            bestDistance = distance;
            result = sample;
            found = true;
        }
    }
    return found;
}

inline bool runtime_history_rolling_average(const RuntimeFeatureHistory &history,
                                            uint32_t currentEpoch, uint32_t windowSeconds,
                                            uint16_t minimumSamples, bool requireFullWindow,
                                            uint32_t boundaryToleranceSeconds,
                                            float &no2AverageHardwareRaw,
                                            float &vocAverageHardwareRaw)
{
    if (!runtime_history_valid(history) || history.count == 0 || currentEpoch == 0)
    {
        return false;
    }

    const uint32_t windowStart = currentEpoch > windowSeconds ? currentEpoch - windowSeconds : 0;
    float no2Sum = 0.0f;
    float vocSum = 0.0f;
    uint16_t sampleCount = 0;
    uint32_t oldestEpoch = UINT32_MAX;

    for (size_t i = 0; i < history.count; ++i)
    {
        const RuntimeHistorySample &sample = history.samples[i];
        if (sample.epoch >= windowStart && sample.epoch <= currentEpoch)
        {
            no2Sum += sample.no2HardwareRaw;
            vocSum += sample.vocHardwareRaw;
            ++sampleCount;
            if (sample.epoch < oldestEpoch)
            {
                oldestEpoch = sample.epoch;
            }
        }
    }

    if (sampleCount < minimumSamples)
    {
        return false;
    }
    if (requireFullWindow &&
        (oldestEpoch == UINT32_MAX || oldestEpoch > windowStart + boundaryToleranceSeconds))
    {
        return false;
    }

    no2AverageHardwareRaw = no2Sum / sampleCount;
    vocAverageHardwareRaw = vocSum / sampleCount;
    return isfinite(no2AverageHardwareRaw) && isfinite(vocAverageHardwareRaw);
}

inline RuntimeFeatureStatus build_runtime_model_features(const RuntimeModel &model,
                                                         const RuntimeFeatureHistory &history,
                                                         uint32_t currentEpoch,
                                                         float currentNo2HardwareRaw,
                                                         float currentVocHardwareRaw,
                                                         float temperature,
                                                         float humidity,
                                                         RuntimeModelFeatures &features)
{
    if (!model.enabled)
    {
        return RuntimeFeatureStatus::MODEL_DISABLED;
    }
    if (!isfinite(currentNo2HardwareRaw) || !isfinite(currentVocHardwareRaw) ||
        !isfinite(temperature) || !isfinite(humidity))
    {
        return RuntimeFeatureStatus::INVALID_CURRENT_INPUT;
    }
    if (currentEpoch == 0 || currentEpoch < model.elapsedDaysOriginEpoch ||
        currentEpoch <= model.lag2Seconds)
    {
        return RuntimeFeatureStatus::INVALID_TIME;
    }

    RuntimeHistorySample lag1 = {};
    RuntimeHistorySample lag2 = {};
    const RuntimeModelCoefficients &coefficients = model.coefficients;
    const bool needsLag1 = coefficients.no2RawLag1 != 0.0f ||
                           coefficients.vocRawLag1 != 0.0f;
    const bool needsLag2 = coefficients.no2RawLag2 != 0.0f ||
                           coefficients.vocRawLag2 != 0.0f;
    const bool needsRolling = coefficients.no2RawRolling != 0.0f ||
                              coefficients.vocRawRolling != 0.0f;
    if (needsLag1 &&
        !runtime_history_nearest(history, currentEpoch - model.lag1Seconds,
                                 model.lagToleranceSeconds, lag1))
    {
        return RuntimeFeatureStatus::LAG_1_MISSING;
    }
    if (needsLag2 &&
        !runtime_history_nearest(history, currentEpoch - model.lag2Seconds,
                                 model.lagToleranceSeconds, lag2))
    {
        return RuntimeFeatureStatus::LAG_2_MISSING;
    }

    float no2RollingHardwareRaw = 0.0f;
    float vocRollingHardwareRaw = 0.0f;
    if (needsRolling &&
        !runtime_history_rolling_average(history, currentEpoch, model.rollingWindowSeconds,
                                         model.minimumRollingSamples,
                                         model.requireFullRollingWindow,
                                         model.lagToleranceSeconds,
                                         no2RollingHardwareRaw, vocRollingHardwareRaw))
    {
        return RuntimeFeatureStatus::ROLLING_WINDOW_INCOMPLETE;
    }

    features.no2Raw = runtime_model_scale_raw(currentNo2HardwareRaw,
                                              model.no2RawScale, model.no2RawOffset);
    features.vocRaw = runtime_model_scale_raw(currentVocHardwareRaw,
                                              model.vocRawScale, model.vocRawOffset);
    features.temperature = temperature;
    features.humidity = humidity;
    features.no2RawLag1 = runtime_model_scale_raw(lag1.no2HardwareRaw,
                                                  model.no2RawScale, model.no2RawOffset);
    features.no2RawLag2 = runtime_model_scale_raw(lag2.no2HardwareRaw,
                                                  model.no2RawScale, model.no2RawOffset);
    features.no2RawRolling = runtime_model_scale_raw(no2RollingHardwareRaw,
                                                     model.no2RawScale, model.no2RawOffset);
    features.vocRawLag1 = runtime_model_scale_raw(lag1.vocHardwareRaw,
                                                  model.vocRawScale, model.vocRawOffset);
    features.vocRawLag2 = runtime_model_scale_raw(lag2.vocHardwareRaw,
                                                  model.vocRawScale, model.vocRawOffset);
    features.vocRawRolling = runtime_model_scale_raw(vocRollingHardwareRaw,
                                                     model.vocRawScale, model.vocRawOffset);
    features.elapsedDays = static_cast<float>(currentEpoch - model.elapsedDaysOriginEpoch) / 86400.0f;
    return RuntimeFeatureStatus::READY;
}

inline float evaluate_runtime_model(const RuntimeModel &model, const RuntimeModelFeatures &f)
{
    const RuntimeModelCoefficients &c = model.coefficients;
    float result = c.intercept;
    result += c.no2Raw * f.no2Raw;
    result += c.vocRaw * f.vocRaw;
    result += c.temperature * f.temperature;
    result += c.humidity * f.humidity;
    result += c.no2RawLag1 * f.no2RawLag1;
    result += c.no2RawLag2 * f.no2RawLag2;
    result += c.no2RawRolling * f.no2RawRolling;
    result += c.vocRawLag1 * f.vocRawLag1;
    result += c.vocRawLag2 * f.vocRawLag2;
    result += c.vocRawRolling * f.vocRawRolling;
    result += c.elapsedDays * f.elapsedDays;
    result += c.no2RawSquared * f.no2Raw * f.no2Raw;
    result += c.no2RawTemperature * f.no2Raw * f.temperature;
    result += c.no2RawHumidity * f.no2Raw * f.humidity;
    result += c.vocRawSquared * f.vocRaw * f.vocRaw;
    result += c.vocRawTemperature * f.vocRaw * f.temperature;
    result += c.vocRawHumidity * f.vocRaw * f.humidity;
    const float temperatureAboveThreshold = f.temperature > model.temperatureThresholdC
                                                ? f.temperature - model.temperatureThresholdC
                                                : 0.0f;
    result += c.temperatureAboveThreshold * temperatureAboveThreshold;

    if (model.clampMinEnabled && result < model.outputClampMin)
    {
        result = model.outputClampMin;
    }
    if (model.clampMaxEnabled && result > model.outputClampMax)
    {
        result = model.outputClampMax;
    }
    return result;
}

inline const char *runtime_feature_status_name(RuntimeFeatureStatus status)
{
    switch (status)
    {
    case RuntimeFeatureStatus::READY:
        return "READY";
    case RuntimeFeatureStatus::MODEL_DISABLED:
        return "MODEL_DISABLED";
    case RuntimeFeatureStatus::INVALID_CURRENT_INPUT:
        return "INVALID_CURRENT_INPUT";
    case RuntimeFeatureStatus::INVALID_TIME:
        return "INVALID_TIME";
    case RuntimeFeatureStatus::LAG_1_MISSING:
        return "LAG_1_MISSING";
    case RuntimeFeatureStatus::LAG_2_MISSING:
        return "LAG_2_MISSING";
    case RuntimeFeatureStatus::ROLLING_WINDOW_INCOMPLETE:
        return "ROLLING_WINDOW_INCOMPLETE";
    }
    return "UNKNOWN";
}
