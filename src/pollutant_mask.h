#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

class PollutantMask
{
public:
    PollutantMask() : bits_(0), count_(0), order_{} {}

    void clear()
    {
        bits_ = 0;
        count_ = 0;
    }

    bool add(const char *name)
    {
        const int index = find(name);
        if (index < 0)
        {
            return false;
        }
        const uint32_t bit = 1UL << index;
        const bool inserted = (bits_ & bit) == 0;
        if (inserted)
        {
            bits_ |= bit;
            order_[count_++] = static_cast<uint8_t>(index);
        }
        return inserted;
    }

    void push_back(const char *name) { add(name); }

    bool contains(const char *name) const
    {
        const int index = find(name);
        return index >= 0 && (bits_ & (1UL << index)) != 0;
    }

    size_t size() const
    {
        return count_;
    }

    const char *operator[](size_t ordinal) const
    {
        return ordinal < count_ ? nameAt(order_[ordinal]) : "";
    }

private:
    static constexpr size_t NAME_COUNT = 28;
    uint32_t bits_;
    uint8_t count_;
    uint8_t order_[NAME_COUNT];

    static const char *nameAt(size_t index)
    {
        static const char *const names[NAME_COUNT] = {
            "HD_co", "HD_no2", "HD_o3", "HD_so2", "Multi_c2h5oh",
            "Multi_co", "Multi_no2", "Multi_voc", "c2h5oh", "co", "co2",
            "direzione_vento", "intensita_vento", "luminosita", "nh3", "no2",
            "nox_index", "o3", "old_o3", "pm1", "pm10", "pm2_5",
            "pressione", "so2", "temperatura", "umidita", "voc", "voc_index"};
        return names[index];
    }

    static int find(const char *name)
    {
        if (name == nullptr)
        {
            return -1;
        }
        for (size_t i = 0; i < NAME_COUNT; ++i)
        {
            if (strcmp(name, nameAt(i)) == 0)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }
};

static_assert(sizeof(PollutantMask) == 36, "PollutantMask layout changed: RAM budget invalid");
