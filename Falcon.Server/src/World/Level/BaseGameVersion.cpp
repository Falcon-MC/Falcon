#include "World/Level/BaseGameVersion.h"

#include <cctype>

namespace {
    BaseGameVersion gCurrent;
}

BaseGameVersion BaseGameVersion::parse(const std::string &text) {
    BaseGameVersion version;
    uint32_t parts[3] = {0, 0, 0};
    size_t part = 0;
    bool digits = false;
    for (const char character: text) {
        if (character == '.') {
            if (!digits || ++part >= 3)
                return BaseGameVersion();
            digits = false;
            continue;
        }
        if (!std::isdigit((unsigned char) character))
            return BaseGameVersion();
        parts[part] = parts[part] * 10 + (uint32_t) (character - '0');
        if (parts[part] > 0xFFFF)
            return BaseGameVersion();
        digits = true;
    }
    if (!digits)
        return BaseGameVersion();

    version.mMajor = (uint16_t) parts[0];
    version.mMinor = (uint16_t) parts[1];
    version.mPatch = (uint16_t) parts[2];
    version.mAny = false;
    return version;
}

const BaseGameVersion &BaseGameVersion::current() {
    return gCurrent;
}

void BaseGameVersion::setCurrent(const BaseGameVersion &version) {
    gCurrent = version;
}

bool BaseGameVersion::isBefore(uint16_t major, uint16_t minor, uint16_t patch) const {
    if (mAny)
        return false;
    if (mMajor != major)
        return mMajor < major;
    if (mMinor != minor)
        return mMinor < minor;
    return mPatch < patch;
}
