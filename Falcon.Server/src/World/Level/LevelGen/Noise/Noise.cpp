#include "World/Level/LevelGen/Noise/Noise.h"

Noise::Noise(IRandom &rand) : mOriginX(0.0), mOriginY(0.0), mOriginZ(0.0), mPermutations() {
    mOriginX = rand.nextDouble() * 256.0;
    mOriginY = rand.nextDouble() * 256.0;
    mOriginZ = rand.nextDouble() * 256.0;

    for (int32_t j = 0; j < 256; ++j) {
        mPermutations[static_cast<size_t>(j)] = static_cast<int8_t>(j);
    }

    for (int32_t index = 0; index < 256; ++index) {
        const int32_t randomIndex = rand.nextBoundedInt(255 - index) + index;
        const int8_t temp = mPermutations[static_cast<size_t>(index)];
        mPermutations[static_cast<size_t>(index)] = mPermutations[static_cast<size_t>(randomIndex)];
        mPermutations[static_cast<size_t>(randomIndex)] = temp;
    }
}

Noise::Noise(std::mt19937 &random) : mOriginX(0.0), mOriginY(0.0), mOriginZ(0.0), mPermutations() {
    mOriginX = static_cast<double>(random()) * 2.3283064365386963e-10 * 256.0;
    mOriginY = static_cast<double>(random()) * 2.3283064365386963e-10 * 256.0;
    mOriginZ = static_cast<double>(random()) * 2.3283064365386963e-10 * 256.0;

    for (int32_t j = 0; j < 256; ++j) {
        mPermutations[static_cast<size_t>(j)] = static_cast<int8_t>(j);
    }

    for (int32_t index = 0; index < 256; ++index) {
        const auto randomIndex = static_cast<int32_t>(random() % static_cast<uint32_t>(256 - index)) + index;
        const int8_t temp = mPermutations[static_cast<size_t>(index)];
        mPermutations[static_cast<size_t>(index)] = mPermutations[static_cast<size_t>(randomIndex)];
        mPermutations[static_cast<size_t>(randomIndex)] = temp;
    }
}

double Noise::getOriginX() const {
    return mOriginX;
}

double Noise::getOriginY() const {
    return mOriginY;
}

double Noise::getOriginZ() const {
    return mOriginZ;
}

int32_t Noise::lookup(int32_t hash) const {
    return static_cast<int32_t>(mPermutations[static_cast<size_t>(hash & 0xFF)]) & 0xFF;
}
