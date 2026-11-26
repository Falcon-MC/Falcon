#include "Core/Text/StringUtil.h"

#include <algorithm>
#include <cctype>

namespace StringUtil {
    std::string toLowerCase(const std::string &value) {
        std::string lowered = value;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                       [](unsigned char character) { return (char) std::tolower(character); });
        return lowered;
    }

    std::string printable(const std::string &text, size_t maximum) {
        std::string out = text.substr(0, maximum);
        for (char &character: out) {
            const unsigned char byte = (unsigned char) character;
            if (byte < 0x20 || byte == 0x7f)
                character = '?';
        }
        if (text.size() > maximum)
            out += "...";
        return out;
    }
}
