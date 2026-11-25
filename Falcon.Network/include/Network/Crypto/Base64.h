#pragma once

#include <string>

namespace Base64 {

    std::string encode(const std::string &data);

    bool decode(const std::string &text, std::string &out);

    std::string encodeUrl(const std::string &data);

    bool decodeUrl(const std::string &text, std::string &out);

}
