#pragma once

#include "World/Level/LevelGen/Random/IRandom.h"
#include "World/Level/LevelGen/Random/XoroshiroPositionalFactory.h"

#include <cstdint>

class OverworldNoiseHolder {
public:
    explicit OverworldNoiseHolder(int64_t seed);

    IRandom &getRandomSource() const { return *mRandomSource; }

    const XoroshiroPositionalFactory &getNoiseFactory() const {
        return mNoiseFactory;
    }

private:
    IRandomPtr mRandomSource;
    XoroshiroPositionalFactory mNoiseFactory;
};
