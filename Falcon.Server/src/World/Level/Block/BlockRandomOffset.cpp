#include "World/Level/Block/BlockRandomOffset.h"

#include <cmath>

namespace {
    constexpr uint64_t SEED_MIX = 0x6A09E667F3BCC909ULL;
    constexpr uint64_t SEED_STEP = 0x9E3779B97F4A7C15ULL;

    uint64_t rotateLeft(uint64_t value, int shift) {
        return (value << shift) | (value >> (64 - shift));
    }

    uint64_t signExtended(int32_t value) {
        return (uint64_t) (int64_t) value;
    }

    uint64_t mixed(uint64_t value) {
        value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
        value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
        return value ^ (value >> 31);
    }

    struct Random {
        uint64_t first;
        uint64_t second;

        // top 24 bits of the next value, as 0..1
        float nextUnit() {
            const uint64_t value = rotateLeft(first + second, 17) + first;
            const uint64_t blend = second ^ first;
            first = rotateLeft(first, 49) ^ blend ^ (blend << 21);
            second = rotateLeft(blend, 28);
            return (float) (value >> 40) * (1.0f / 16777216.0f);
        }
    };

    Random seeded(int32_t x, int32_t z) {
        const uint64_t hash = (signExtended(x) * 3129871ULL) ^ (signExtended(z) * 116129781ULL);
        const uint64_t product = hash * (42317861ULL * hash + 11);
        const uint64_t seed = signExtended((int32_t) (uint32_t) (product >> 16)) ^ SEED_MIX;
        Random random{mixed(seed), mixed(seed + SEED_STEP)};
        if ((random.first | random.second) == 0)
            random = {SEED_STEP, SEED_MIX};
        return random;
    }

    // every intermediate is a float in the game: keeps the compiler from fusing a multiply into an add
    float rounded(float value) {
        volatile float stored = value;
        return stored;
    }

    float sample(float minimum, float maximum, uint32_t steps, float unit) {
        if (!(minimum < maximum))
            return minimum;
        if (steps == 1)
            return rounded(rounded(minimum + maximum) * 0.5f);
        const float difference = rounded(maximum - minimum);
        if (steps == 0)
            return rounded(rounded(difference * unit) + minimum);
        const float index = std::floor(rounded((float) steps * unit));
        const float step = rounded(difference / (float) (steps - 1));
        return rounded(rounded(index * step) + minimum);
    }
}

Vector3f BlockRandomOffset::horizontal(int32_t x, int32_t z, float range, uint32_t steps) {
    Random random = seeded(x, z);
    const float offsetX = sample(-range, range, steps, random.nextUnit());
    // the vertical axis draws too, even with nothing to shift
    random.nextUnit();
    const float offsetZ = sample(-range, range, steps, random.nextUnit());
    return Vector3f(offsetX, 0.0f, offsetZ);
}
