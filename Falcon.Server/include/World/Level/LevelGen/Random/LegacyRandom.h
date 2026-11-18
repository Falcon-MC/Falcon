#pragma once

#include <cstdint>

// the game's 48 bit linear congruential random, seeded with the scramble
class LegacyRandom {
public:
    explicit LegacyRandom(int64_t seed) : mState(((uint64_t) seed ^ MULTIPLIER) & MASK) {}

    int32_t next(int bits) {
        mState = (mState * MULTIPLIER + 11) & MASK;
        return (int32_t) (mState >> (48 - bits));
    }

    int64_t nextLong() {
        const int64_t high = next(32);
        const int64_t low = next(32);
        return (int64_t) ((uint64_t) high << 32) + low;
    }

    int32_t nextInt(int32_t bound) {
        if ((bound & -bound) == bound)
            return (int32_t) (((int64_t) bound * (int64_t) next(31)) >> 31);
        int32_t bits;
        int32_t value;
        do {
            bits = next(31);
            value = bits % bound;
        } while ((int32_t) ((uint32_t) bits - (uint32_t) value + (uint32_t) (bound - 1)) < 0);
        return value;
    }

private:
    static constexpr uint64_t MULTIPLIER = 0x5DEECE66DULL;
    static constexpr uint64_t MASK = (1ULL << 48) - 1ULL;
    uint64_t mState;
};
