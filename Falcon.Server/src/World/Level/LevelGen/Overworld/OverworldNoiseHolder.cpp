#include "World/Level/LevelGen/Overworld/OverworldNoiseHolder.h"

#include "World/Level/LevelGen/Random/XoroshiroRandom.h"

namespace {
    XoroshiroPositionalFactory createNoiseFactory(int64_t seed) {
        XoroshiroRandom random(seed);
        return random.forkPositional();
    }
}

OverworldNoiseHolder::OverworldNoiseHolder(int64_t seed)
    : mRandomSource(std::make_shared<XoroshiroRandom>(seed)), mNoiseFactory(createNoiseFactory(seed)) {
}
