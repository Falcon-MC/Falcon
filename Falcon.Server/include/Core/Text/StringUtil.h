#pragma once

#include <string>

namespace StringUtil {
    std::string toLowerCase(const std::string &value);

    // client text for a log line: no control bytes (a terminal obeys escape sequences), bounded
    std::string printable(const std::string &text, size_t maximum = 256);
}
