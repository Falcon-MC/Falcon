#pragma once

#include <cstdint>
#include <string>

// the base game version StartGame announces; the client builds its block shapes from it
class BaseGameVersion {
public:
    static BaseGameVersion parse(const std::string &text);

    static const BaseGameVersion &current();

    static void setCurrent(const BaseGameVersion &version);

    // SemVersion::operator< (sub_1403A2750): "*" is never older
    bool isBefore(uint16_t major, uint16_t minor, uint16_t patch) const;

private:
    uint16_t mMajor = 0;
    uint16_t mMinor = 0;
    uint16_t mPatch = 0;
    bool mAny = true;
};
