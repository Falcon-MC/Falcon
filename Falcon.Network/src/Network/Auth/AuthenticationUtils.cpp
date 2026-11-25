#include "Network/Auth/AuthenticationUtils.h"

#include <openssl/rand.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <random>

namespace {

    int64_t daysFromCivil(int64_t year, unsigned month, unsigned day) {
        year -= month <= 2 ? 1 : 0;
        const int64_t era = (year >= 0 ? year : year - 399) / 400;
        const unsigned yearOfEra = (unsigned) (year - era * 400);
        const unsigned dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
        const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
        return era * 146097 + (int64_t) dayOfEra - 719468;
    }

    bool fillRandom(unsigned char *buffer, size_t length) {
        if (RAND_bytes(buffer, (int) length) == 1)
            return true;

        std::random_device device;
        for (size_t index = 0; index < length; ++index)
            buffer[index] = (unsigned char) (device() & 0xff);

        return true;
    }

    int monthFromName(const char *name) {
        static const char *const MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                             "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

        for (int index = 0; index < 12; ++index) {
            if (strncmp(name, MONTHS[index], 3) == 0)
                return index + 1;
        }

        return 0;
    }

}

namespace AuthenticationUtils {

    int64_t currentUnixTime() {
        return (int64_t) std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
    }

    std::string generateUuid() {
        unsigned char bytes[16];
        fillRandom(bytes, sizeof(bytes));

        bytes[6] = (unsigned char) ((bytes[6] & 0x0f) | 0x40);
        bytes[8] = (unsigned char) ((bytes[8] & 0x3f) | 0x80);

        char buffer[40];
        snprintf(buffer, sizeof(buffer),
                 "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                 bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
                 bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
        return std::string(buffer);
    }

    std::string randomHex(size_t byteCount) {
        static const char HEX[] = "0123456789abcdef";

        std::string bytes(byteCount, '\0');
        fillRandom((unsigned char *) &bytes[0], byteCount);

        std::string result;
        result.reserve(byteCount * 2);

        for (char value: bytes) {
            result.push_back(HEX[((unsigned char) value >> 4) & 0x0f]);
            result.push_back(HEX[(unsigned char) value & 0x0f]);
        }

        return result;
    }

    bool parseIso8601(const std::string &text, int64_t &outUnixTime) {
        int year = 0;
        unsigned month = 0;
        unsigned day = 0;
        unsigned hour = 0;
        unsigned minute = 0;
        unsigned second = 0;

        if (sscanf(text.c_str(), "%d-%u-%uT%u:%u:%u", &year, &month, &day, &hour, &minute, &second) != 6)
            return false;

        if (month < 1 || month > 12 || day < 1 || day > 31)
            return false;

        int64_t offsetSeconds = 0;
        const size_t timePosition = text.find('T');
        const size_t zonePosition = text.find_first_of("+-", timePosition == std::string::npos ? 0 : timePosition);

        if (zonePosition != std::string::npos) {
            unsigned zoneHours = 0;
            unsigned zoneMinutes = 0;

            if (sscanf(text.c_str() + zonePosition + 1, "%u:%u", &zoneHours, &zoneMinutes) >= 1) {
                offsetSeconds = (int64_t) zoneHours * 3600 + (int64_t) zoneMinutes * 60;
                if (text[zonePosition] == '-')
                    offsetSeconds = -offsetSeconds;
            }
        }

        outUnixTime = daysFromCivil(year, month, day) * 86400 + (int64_t) hour * 3600 + (int64_t) minute * 60 +
                      (int64_t) second - offsetSeconds;
        return true;
    }

    bool parseHttpDate(const std::string &text, int64_t &outUnixTime) {
        char weekday[8] = {0};
        char monthName[8] = {0};
        unsigned day = 0;
        int year = 0;
        unsigned hour = 0;
        unsigned minute = 0;
        unsigned second = 0;

        if (sscanf(text.c_str(), "%7[^,], %u %7s %d %u:%u:%u", weekday, &day, monthName, &year, &hour, &minute,
                   &second) != 7)
            return false;

        const int month = monthFromName(monthName);
        if (month == 0 || day < 1 || day > 31)
            return false;

        outUnixTime = daysFromCivil(year, (unsigned) month, day) * 86400 + (int64_t) hour * 3600 +
                      (int64_t) minute * 60 + (int64_t) second;
        return true;
    }

    std::string urlEncode(const std::string &value) {
        static const char HEX[] = "0123456789ABCDEF";

        std::string result;
        result.reserve(value.size() * 3);

        for (char character: value) {
            const unsigned char byte = (unsigned char) character;

            if ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9') ||
                byte == '-' || byte == '_' || byte == '.' || byte == '~') {
                result.push_back(character);
                continue;
            }

            result.push_back('%');
            result.push_back(HEX[byte >> 4]);
            result.push_back(HEX[byte & 0x0f]);
        }

        return result;
    }

    std::string joinUrl(const std::string &base, const std::string &path) {
        std::string result = base;

        while (!result.empty() && result.back() == '/')
            result.pop_back();

        if (path.empty() || path.front() != '/')
            result.push_back('/');

        result += path;
        return result;
    }

}
