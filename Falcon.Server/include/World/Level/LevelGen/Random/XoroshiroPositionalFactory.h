#pragma once

#include "World/Level/LevelGen/Random/XoroshiroRandom.h"

#include <cstdint>
#include <string>

class XoroshiroPositionalFactory {
public:
    XoroshiroPositionalFactory(int64_t lo, int64_t hi);

    // md5 of the name, read as two big-endian longs
    XoroshiroRandom hashOf(const std::string &name) const;

    XoroshiroRandom at(int32_t x, int32_t y, int32_t z) const;

    static int64_t blockSeed(int32_t x, int32_t y, int32_t z);

private:
    int64_t mLo;
    int64_t mHi;
};
