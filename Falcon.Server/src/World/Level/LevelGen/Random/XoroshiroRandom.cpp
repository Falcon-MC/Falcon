#include "World/Level/LevelGen/Random/XoroshiroRandom.h"

#include "World/Level/LevelGen/Random/XoroshiroPositionalFactory.h"

#include <cmath>

namespace {

    constexpr int64_t GOLDEN_RATIO_64 = static_cast<int64_t>(0x9E3779B97F4A7C15ULL);
    constexpr int64_t SILVER_RATIO_64 = static_cast<int64_t>(0x6A09E667F3BCC909ULL);

    // signed 64-bit arithmetic with logical shifts: everything goes through uint64_t, overflow stays defined
    int64_t unsignedShiftRight(int64_t value, int32_t bits) {
        return static_cast<int64_t>(static_cast<uint64_t>(value) >> bits);
    }

    int64_t mixStafford13(int64_t value) {
        uint64_t z = static_cast<uint64_t>(value);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return static_cast<int64_t>(z ^ (z >> 31));
    }

}

XoroshiroRandom::XoroshiroRandom(int64_t seed)
        : mSeed(0), mInitialLo(0), mInitialHi(0), mS0(0), mS1(0), mNextGaussian(0.0), mHaveNextGaussian(false) {
    setSeed(seed);
}

XoroshiroRandom::XoroshiroRandom(int64_t seed, int64_t lo, int64_t hi)
        : mSeed(seed), mInitialLo(0), mInitialHi(0), mS0(0), mS1(0), mNextGaussian(0.0), mHaveNextGaussian(false) {
    _setState(lo, hi);
}

XoroshiroRandom XoroshiroRandom::fromState(int64_t lo, int64_t hi) {
    return XoroshiroRandom(0, lo, hi);
}

XoroshiroPositionalFactory XoroshiroRandom::forkPositional() {
    const int64_t lo = nextLong();
    const int64_t hi = nextLong();
    return XoroshiroPositionalFactory(lo, hi);
}

std::shared_ptr<IRandom> XoroshiroRandom::fork() {
    return std::make_shared<XoroshiroRandom>(nextLong());
}

std::shared_ptr<IRandom> XoroshiroRandom::identical() const {
    return std::shared_ptr<XoroshiroRandom>(new XoroshiroRandom(mSeed, mInitialLo, mInitialHi));
}

int32_t XoroshiroRandom::nextInt() {
    return static_cast<int32_t>(nextLong());
}

int32_t XoroshiroRandom::nextInt(int32_t max) {
    // lenient for feature placement callers
    if (max <= 0) {
        return 0;
    }

    return nextExclusiveInt(max);
}

int32_t XoroshiroRandom::nextInt(int32_t min, int32_t max) {
    return min + nextExclusiveInt(max - min + 1);
}

int32_t XoroshiroRandom::nextBoundedInt(int32_t max) {
    return nextExclusiveInt(max + 1);
}

int32_t XoroshiroRandom::nextExclusiveInt(int32_t bound) {
    if (bound <= 0) {
        return 0;
    }

    const uint64_t unsignedBound = static_cast<uint32_t>(bound);
    uint64_t product = (static_cast<uint64_t>(nextLong()) & 0xFFFFFFFFULL) * unsignedBound;
    uint64_t low = product & 0xFFFFFFFFULL;
    if (low < unsignedBound) {
        const uint64_t threshold = static_cast<uint32_t>(-static_cast<uint32_t>(bound)) % static_cast<uint32_t>(bound);
        while (low < threshold) {
            product = (static_cast<uint64_t>(nextLong()) & 0xFFFFFFFFULL) * unsignedBound;
            low = product & 0xFFFFFFFFULL;
        }
    }

    return static_cast<int32_t>(product >> 32);
}

int64_t XoroshiroRandom::nextLong() {
    int64_t i = mS0;
    int64_t j = mS1;
    int64_t k = static_cast<int64_t>(
            static_cast<uint64_t>(
                    _rotateLeft(static_cast<int64_t>(static_cast<uint64_t>(i) + static_cast<uint64_t>(j)), 17))
            + static_cast<uint64_t>(i));
    j ^= i;
    mS0 = _rotateLeft(i, 49) ^ j ^ static_cast<int64_t>(static_cast<uint64_t>(j) << 21);
    mS1 = _rotateLeft(j, 28);
    return k;
}

bool XoroshiroRandom::nextBoolean() {
    return (nextLong() & 1LL) != 0LL;
}

float XoroshiroRandom::nextFloat() {
    return static_cast<float>(static_cast<double>(unsignedShiftRight(nextLong(), 40))
                              * (1.0 / static_cast<double>(1LL << 24)));
}

double XoroshiroRandom::nextDouble() {
    int64_t bits = unsignedShiftRight(nextLong(), 11);
    return static_cast<double>(bits) * (1.0 / static_cast<double>(1LL << 53));
}

double XoroshiroRandom::nextGaussian() {
    // marsaglia polar method, second value cached like the game
    if (mHaveNextGaussian) {
        mHaveNextGaussian = false;
        return mNextGaussian;
    }

    double v1;
    double v2;
    double s;
    do {
        v1 = 2.0 * nextDouble() - 1.0;
        v2 = 2.0 * nextDouble() - 1.0;
        s = v1 * v1 + v2 * v2;
    } while (s >= 1.0 || s == 0.0);

    const double multiplier = std::sqrt(-2.0 * std::log(s) / s);
    mNextGaussian = v2 * multiplier;
    mHaveNextGaussian = true;
    return v1 * multiplier;
}

void XoroshiroRandom::setSeed(int64_t seed) {
    mSeed = seed;
    const int64_t lo = seed ^ SILVER_RATIO_64;
    const int64_t hi = static_cast<int64_t>(static_cast<uint64_t>(lo) + static_cast<uint64_t>(GOLDEN_RATIO_64));
    _setState(mixStafford13(lo), mixStafford13(hi));
}

int64_t XoroshiroRandom::getSeed() const {
    return mSeed;
}

void XoroshiroRandom::_setState(int64_t lo, int64_t hi) {
    // all-zero state: xoroshiro would emit zeros forever
    if ((lo | hi) == 0) {
        lo = GOLDEN_RATIO_64;
        hi = SILVER_RATIO_64;
    }

    mInitialLo = lo;
    mInitialHi = hi;
    mS0 = lo;
    mS1 = hi;
    mHaveNextGaussian = false;
}

int64_t XoroshiroRandom::_rotateLeft(int64_t x, int32_t k) {
    uint64_t value = static_cast<uint64_t>(x);
    return static_cast<int64_t>((value << k) | (value >> (64 - k)));
}
