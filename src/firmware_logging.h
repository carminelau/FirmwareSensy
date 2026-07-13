#pragma once

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"

class FirmwareSerialClass
{
public:
    explicit FirmwareSerialClass(HardwareSerial &serial) : serial_(serial) {}

    void begin(unsigned long baudRate) { serial_.begin(baudRate); }
    int available() { return serial_.available(); }
    String readStringUntil(char terminator) { return serial_.readStringUntil(terminator); }
    void flush() { serial_.flush(); }

    template <typename T>
    size_t print(const T &value)
    {
#if FW_LOG_LEVEL >= 3
        return serial_.print(value);
#else
        (void)value;
        return 0;
#endif
    }

    template <typename T>
    size_t print(const T &value, int format)
    {
#if FW_LOG_LEVEL >= 3
        return serial_.print(value, format);
#else
        (void)value;
        (void)format;
        return 0;
#endif
    }

    size_t println()
    {
#if FW_LOG_LEVEL >= 3
        return serial_.println();
#else
        return 0;
#endif
    }

    template <size_t N>
    size_t println(const char (&message)[N])
    {
#if FW_LOG_LEVEL >= 3
        return writeLine(message);
#elif FW_LOG_LEVEL >= 1
        return isReleaseMessage(message) ? serial_.println(message) : 0;
#else
        (void)message;
        return 0;
#endif
    }

    size_t println(const char *message)
    {
#if FW_LOG_LEVEL >= 3
        return writeLine(message);
#elif FW_LOG_LEVEL >= 1
        return isReleaseMessage(message) ? serial_.println(message) : 0;
#else
        (void)message;
        return 0;
#endif
    }

    size_t println(const String &message)
    {
#if FW_LOG_LEVEL >= 3
        return writeLine(message.c_str());
#elif FW_LOG_LEVEL >= 1
        return isReleaseMessage(message.c_str()) ? serial_.println(message) : 0;
#else
        (void)message;
        return 0;
#endif
    }

    template <typename T>
    size_t println(const T &value)
    {
#if FW_LOG_LEVEL >= 3
        return serial_.println(value);
#else
        (void)value;
        return 0;
#endif
    }

    template <typename T>
    size_t println(const T &value, int format)
    {
#if FW_LOG_LEVEL >= 3
        return serial_.println(value, format);
#else
        (void)value;
        (void)format;
        return 0;
#endif
    }

    template <size_t N, typename... Args>
    int printf(const char (&format)[N], Args... args)
    {
#if FW_LOG_LEVEL >= 3
#if FW_LOG_BILINGUAL
        char message[384];
        const int written = snprintf(message, sizeof(message), format, args...);
        size_t length = strlen(message);
        bool hadNewline = false;
        while (length > 0 && (message[length - 1] == '\n' || message[length - 1] == '\r'))
        {
            message[--length] = '\0';
            hadNewline = true;
        }
        if (hadNewline)
        {
            writeLine(message);
        }
        else
        {
            serial_.print(message);
        }
        return written;
#else
        return serial_.printf(format, args...);
#endif
#elif FW_LOG_LEVEL >= 1
        return isReleaseMessage(format) ? serial_.printf(format, args...) : 0;
#else
        (void)format;
        return 0;
#endif
    }

private:
    HardwareSerial &serial_;

    static bool startsWith(const char *message, const char *prefix)
    {
        return message != nullptr && strncmp(message, prefix, strlen(prefix)) == 0;
    }

    static bool isReleaseMessage(const char *message)
    {
        return startsWith(message, "ERROR") || startsWith(message, "WARNING") ||
               startsWith(message, "CRITICAL") || startsWith(message, "[ERROR]") ||
               startsWith(message, "[WARNING]") || startsWith(message, "[BOOT]") ||
               startsWith(message, "[CONFIG]") || startsWith(message, "[STATUS]") ||
               startsWith(message, "[NETWORK]") || startsWith(message, "[OTA]") ||
               startsWith(message, "[SAVED]") || startsWith(message, "[RUNTIME]") ||
               startsWith(message, "[SENSORS]") || startsWith(message, "[CYCLE]") ||
               startsWith(message, "[HTTP]") || startsWith(message, "[STORAGE]") ||
               startsWith(message, "[SNIFFER]");
    }

    size_t writeLine(const char *message)
    {
        if (message == nullptr)
        {
            return serial_.println();
        }
#if FW_LOG_BILINGUAL
        const char *translation = translateCritical(message);
        if (translation != nullptr)
        {
            size_t written = serial_.print(message);
            written += serial_.print(" | EN: ");
            written += serial_.println(translation);
            return written;
        }
#endif
        return serial_.println(message);
    }

    static const char *translateCritical(const char *message)
    {
        if (strstr(message, "Fallimento") != nullptr || strstr(message, "fallito") != nullptr ||
            strstr(message, "fallita") != nullptr)
            return "Failure";
        if (strstr(message, "Errore") != nullptr || strstr(message, "ERROR") != nullptr)
            return "Error";
        if (strstr(message, "Riavvio") != nullptr || strstr(message, "riavvio") != nullptr)
            return "Reboot";
        if (strstr(message, "Timeout") != nullptr || strstr(message, "timeout") != nullptr)
            return "Timeout";
        if (strstr(message, "non connesso") != nullptr || strstr(message, "offline") != nullptr)
            return "Not connected";
        if (strstr(message, "completata") != nullptr || strstr(message, "COMPLETED") != nullptr)
            return "Completed";
        return nullptr;
    }
};

static HardwareSerial &FirmwareRawSerial = ::Serial;
static FirmwareSerialClass FirmwareSerial(FirmwareRawSerial);

#define Serial FirmwareSerial
