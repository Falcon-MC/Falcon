#include "Level/Generator/Random/XoroshiroRandom.h"

#include "Core/Math/MathConstants.h"

#include <cmath>

namespace {

constexpr int64_t GOLDEN_GAMMA = static_cast<int64_t>(0x9E3779B97F4A7C15ULL);
constexpr int64_t MIX_CONSTANT_1 = static_cast<int64_t>(0xBF58476D1CE4E5B9ULL);
constexpr int64_t MIX_CONSTANT_2 = static_cast<int64_t>(0x94D049BB133111EBULL);
constexpr int32_t JAVA_INTEGER_MAX_VALUE = 0x7FFFFFFF;

int64_t unsignedShiftRight(int64_t value, int32_t bits) {
    return static_cast<int64_t>(static_cast<uint64_t>(value) >> bits);
}

}

XoroshiroRandom::XoroshiroRandom(int64_t seed)
    : mSeed(0), mS0(0), mS1(0) {
    setSeed(seed);
}

std::shared_ptr<IRandom> XoroshiroRandom::fork() {
    return std::make_shared<XoroshiroRandom>(nextLong());
}

std::shared_ptr<IRandom> XoroshiroRandom::identical() const {
    return std::make_shared<XoroshiroRandom>(mSeed);
}

int32_t XoroshiroRandom::nextInt() {
    return static_cast<int32_t>(nextLong()) & JAVA_INTEGER_MAX_VALUE;
}

int32_t XoroshiroRandom::nextInt(int32_t max) {
    if (max == 0) {
        return 0;
    }

    return nextInt() % max;
}

int32_t XoroshiroRandom::nextInt(int32_t min, int32_t max) {
    return min + (nextInt() % (max - min));
}

int32_t XoroshiroRandom::nextBoundedInt(int32_t max) {
    return nextInt(max + 1);
}

int64_t XoroshiroRandom::nextLong() {
    int64_t i = mS0;
    int64_t j = mS1;
    int64_t k = static_cast<int64_t>(static_cast<uint64_t>(_rotateLeft(static_cast<int64_t>(static_cast<uint64_t>(i) + static_cast<uint64_t>(j)), 17)) + static_cast<uint64_t>(i));
    j ^= i;
    mS0 = _rotateLeft(i, 49) ^ j ^ static_cast<int64_t>(static_cast<uint64_t>(j) << 21);
    mS1 = _rotateLeft(j, 28);
    return k;
}

bool XoroshiroRandom::nextBoolean() {
    return (nextLong() & 1LL) != 0LL;
}

float XoroshiroRandom::nextFloat() {
    return static_cast<float>(static_cast<double>(unsignedShiftRight(nextLong(), 40)) * (1.0 / static_cast<double>(1LL << 24)));
}

double XoroshiroRandom::nextDouble() {
    int64_t bits = unsignedShiftRight(nextLong(), 11);
    return static_cast<double>(bits) * (1.0 / static_cast<double>(1LL << 53));
}

double XoroshiroRandom::nextGaussian() {
    double u1 = nextDouble();
    double u2 = nextDouble();
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * MathConstants::PI * u2);
}

void XoroshiroRandom::setSeed(int64_t seed) {
    mSeed = seed;

    int64_t state[2] = {0, 0};
    uint64_t z = static_cast<uint64_t>(seed);

    for (int32_t i = 0; i < 2; ++i) {
        z += static_cast<uint64_t>(GOLDEN_GAMMA);
        uint64_t r = z;
        r = (r ^ (r >> 30)) * static_cast<uint64_t>(MIX_CONSTANT_1);
        r = (r ^ (r >> 27)) * static_cast<uint64_t>(MIX_CONSTANT_2);
        r ^= (r >> 31);
        state[i] = static_cast<int64_t>(r);
    }

    if (state[0] == 0 && state[1] == 0) {
        state[0] = GOLDEN_GAMMA;
        state[1] = ~state[0];
    }

    mS0 = state[0];
    mS1 = state[1];
}

int64_t XoroshiroRandom::getSeed() const {
    return mSeed;
}

int64_t XoroshiroRandom::_rotateLeft(int64_t x, int32_t k) {
    uint64_t value = static_cast<uint64_t>(x);
    return static_cast<int64_t>((value << k) | (value >> (64 - k)));
}
