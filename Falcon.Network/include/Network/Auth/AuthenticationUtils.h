#pragma once

#include <cstdint>
#include <string>

namespace AuthenticationUtils {

    int64_t currentUnixTime();

    std::string generateUuid();

    std::string randomHex(size_t byteCount);

    bool parseIso8601(const std::string &text, int64_t &outUnixTime);

    bool parseHttpDate(const std::string &text, int64_t &outUnixTime);

    std::string urlEncode(const std::string &value);

    std::string joinUrl(const std::string &base, const std::string &path);

}
