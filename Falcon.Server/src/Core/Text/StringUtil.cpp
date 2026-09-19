#include "Core/Text/StringUtil.h"

#include <algorithm>
#include <cctype>

namespace StringUtil {
    std::string toLowerCase(const std::string &value) {
        std::string lowered = value;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                       [](unsigned char character) {
                           return (char) std::tolower(character);
                       });
        return lowered;
    }
}
