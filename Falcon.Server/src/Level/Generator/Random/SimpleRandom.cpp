#include "Level/Generator/Random/SimpleRandom.h"

#include "Core/Math/MathConstants.h"

#include <cmath>

namespace {

constexpr int32_t UPPER_MASK = static_cast<int32_t>(0x80000000);
constexpr int32_t LOWER_MASK = 0x7FFFFFFF;
constexpr int32_t MAG01[2] = {0x0, static_cast<int32_t>(0x9908B0DF)};
constexpr int32_t INIT_STATE_SEED = 19650218;
constexpr int32_t INIT_MULTIPLIER = 1812433253;
constexpr int64_t MIX_MULTIPLIER_1 = 1664525LL;
constexpr int64_t MIX_MULTIPLIER_2 = 1566083941LL;
constexpr uint64_t GOLDEN_RATIO_64 = 0x9E3779B97F4A7C15ULL;
constexpr uint64_t NEGATED_GOLDEN_RATIO_64 = 0x61C8864680B583EBULL;
constexpr uint64_t STAFFORD_MULTIPLIER_1 = 0xBF58476D1CE4E5B9ULL;
constexpr uint64_t STAFFORD_MULTIPLIER_2 = 0x94D049BB133111EBULL;
constexpr int32_t EMPTY_BOOLEAN_SOURCE = 1;

int32_t unsignedShiftRight(int32_t value, int32_t bits) {
    return static_cast<int32_t>(static_cast<uint32_t>(value) >> bits);
}

int32_t wrapAdd(int32_t a, int32_t b) {
    return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

int32_t wrapSubtract(int32_t a, int32_t b) {
    return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

int64_t wrapAddLong(int64_t a, int64_t b) {
    return static_cast<int64_t>(static_cast<uint64_t>(a) + static_cast<uint64_t>(b));
}

int64_t wrapSubtractLong(int64_t a, int64_t b) {
    return static_cast<int64_t>(static_cast<uint64_t>(a) - static_cast<uint64_t>(b));
}

int64_t wrapMultiplyLong(int64_t a, int64_t b) {
    return static_cast<int64_t>(static_cast<uint64_t>(a) * static_cast<uint64_t>(b));
}

int64_t toUnsignedLong(int32_t value) {
    return static_cast<int64_t>(static_cast<uint32_t>(value));
}

double makeDouble(int32_t v, int32_t w) {
    const int64_t high = static_cast<int64_t>(static_cast<uint64_t>(static_cast<uint32_t>(unsignedShiftRight(v, 6))) << 27);
    const int32_t low = unsignedShiftRight(w, 5);
    return static_cast<double>(high | static_cast<int64_t>(low)) * 0x1.0p-53;
}

uint64_t stafford13(uint64_t z) {
    z = (z ^ (z >> 30)) * STAFFORD_MULTIPLIER_1;
    z = (z ^ (z >> 27)) * STAFFORD_MULTIPLIER_2;
    return z ^ (z >> 31);
}

}

MersenneTwister::MersenneTwister()
    : mMt(), mMti(N), mMixIndex(1), mBooleanSource(EMPTY_BOOLEAN_SOURCE) {
    setSeed(static_cast<int64_t>(0));
}

std::vector<int32_t> MersenneTwister::_expandSeed(int64_t seed, int32_t length) {
    uint64_t v = static_cast<uint64_t>(seed);

    if (v == NEGATED_GOLDEN_RATIO_64) {
        v = ~v;
    }

    std::vector<int32_t> output(static_cast<size_t>(length), 0);
    const int32_t pairs = length & ~1;

    for (int32_t i = 0; i < pairs; i += 2) {
        v += GOLDEN_RATIO_64;
        const uint64_t x = stafford13(v);
        output[static_cast<size_t>(i)] = static_cast<int32_t>(static_cast<uint32_t>(x & 0xFFFFFFFFULL));
        output[static_cast<size_t>(i + 1)] = static_cast<int32_t>(static_cast<uint32_t>(x >> 32));
    }

    if (pairs < length) {
        const uint64_t x = stafford13(v + GOLDEN_RATIO_64);
        output[static_cast<size_t>(pairs)] = static_cast<int32_t>(static_cast<uint32_t>(x & 0xFFFFFFFFULL));
    }

    return output;
}

void MersenneTwister::setSeed(int64_t seed) {
    setSeed(_expandSeed(seed, N));
}

void MersenneTwister::setSeed(const std::vector<int32_t> &seed) {
    mBooleanSource = EMPTY_BOOLEAN_SOURCE;

    _initState();

    if (seed.empty()) {
        _mixSeedAndState(std::vector<int32_t>(1, 0));
    } else {
        _mixSeedAndState(seed);
    }

    _mixState();

    mMt[0] = UPPER_MASK;
    mMti = N;
}

void MersenneTwister::_initState() {
    int64_t v = static_cast<int64_t>(INIT_STATE_SEED);
    mMt[0] = static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(v) & 0xFFFFFFFFULL));

    for (int32_t k = 1; k < N; ++k) {
        v = wrapAddLong(wrapMultiplyLong(static_cast<int64_t>(INIT_MULTIPLIER), v ^ (v >> 30)), static_cast<int64_t>(k)) & 0xFFFFFFFFLL;
        mMt[k] = static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(v) & 0xFFFFFFFFULL));
    }
}

void MersenneTwister::_mixSeedAndState(const std::vector<int32_t> &seed) {
    const int32_t seedLength = static_cast<int32_t>(seed.size());
    int32_t i = 1;
    int32_t j = 0;

    for (int32_t k = (N > seedLength ? N : seedLength); k > 0; --k) {
        const int64_t l0 = (static_cast<int64_t>(mMt[i]) & 0x7FFFFFFFLL) | (mMt[i] < 0 ? 0x80000000LL : 0x0LL);
        const int64_t l1 = (static_cast<int64_t>(mMt[i - 1]) & 0x7FFFFFFFLL) | (mMt[i - 1] < 0 ? 0x80000000LL : 0x0LL);
        const int64_t mixed = wrapMultiplyLong(l1 ^ (l1 >> 30), MIX_MULTIPLIER_1);
        const int64_t l = wrapAddLong(wrapAddLong(l0 ^ mixed, static_cast<int64_t>(seed[j])), static_cast<int64_t>(j));

        mMt[i] = static_cast<int32_t>(static_cast<uint64_t>(l) & 0xFFFFFFFFULL);
        ++i;
        ++j;

        if (i >= N) {
            mMt[0] = mMt[N - 1];
            i = 1;
        }

        if (j >= seedLength) {
            j = 0;
        }
    }

    mMixIndex = i;
}

void MersenneTwister::_mixState() {
    int32_t i = mMixIndex;

    for (int32_t k = N - 1; k > 0; --k) {
        const int64_t l0 = (static_cast<int64_t>(mMt[i]) & 0x7FFFFFFFLL) | (mMt[i] < 0 ? 0x80000000LL : 0x0LL);
        const int64_t l1 = (static_cast<int64_t>(mMt[i - 1]) & 0x7FFFFFFFLL) | (mMt[i - 1] < 0 ? 0x80000000LL : 0x0LL);
        const int64_t mixed = wrapMultiplyLong(l1 ^ (l1 >> 30), MIX_MULTIPLIER_2);
        const int64_t l = wrapSubtractLong(l0 ^ mixed, static_cast<int64_t>(i));

        mMt[i] = static_cast<int32_t>(static_cast<uint64_t>(l) & 0xFFFFFFFFULL);
        ++i;

        if (i >= N) {
            mMt[0] = mMt[N - 1];
            i = 1;
        }
    }

    mMixIndex = i;
}

int32_t MersenneTwister::nextInt() {
    int32_t y = 0;

    if (mMti >= N) {
        int32_t mtNext = mMt[0];

        for (int32_t k = 0; k < N - M; ++k) {
            const int32_t mtCurr = mtNext;
            mtNext = mMt[k + 1];
            y = (mtCurr & UPPER_MASK) | (mtNext & LOWER_MASK);
            mMt[k] = mMt[k + M] ^ unsignedShiftRight(y, 1) ^ MAG01[y & 0x1];
        }

        for (int32_t k = N - M; k < N - 1; ++k) {
            const int32_t mtCurr = mtNext;
            mtNext = mMt[k + 1];
            y = (mtCurr & UPPER_MASK) | (mtNext & LOWER_MASK);
            mMt[k] = mMt[k + (M - N)] ^ unsignedShiftRight(y, 1) ^ MAG01[y & 0x1];
        }

        y = (mtNext & UPPER_MASK) | (mMt[0] & LOWER_MASK);
        mMt[N - 1] = mMt[M - 1] ^ unsignedShiftRight(y, 1) ^ MAG01[y & 0x1];

        mMti = 0;
    }

    y = mMt[mMti];
    ++mMti;

    y ^= unsignedShiftRight(y, 11);
    y ^= static_cast<int32_t>(static_cast<uint32_t>(y) << 7) & static_cast<int32_t>(0x9D2C5680);
    y ^= static_cast<int32_t>(static_cast<uint32_t>(y) << 15) & static_cast<int32_t>(0xEFC60000);
    y ^= unsignedShiftRight(y, 18);

    return y;
}

int32_t MersenneTwister::nextInt(int32_t n) {
    uint64_t m = static_cast<uint64_t>(static_cast<uint32_t>(nextInt())) * static_cast<uint64_t>(static_cast<uint32_t>(n));
    uint64_t l = m & 0xFFFFFFFFULL;

    if (l < static_cast<uint64_t>(n)) {
        const uint64_t t = 0x100000000ULL % static_cast<uint64_t>(n);

        while (l < t) {
            m = static_cast<uint64_t>(static_cast<uint32_t>(nextInt())) * static_cast<uint64_t>(static_cast<uint32_t>(n));
            l = m & 0xFFFFFFFFULL;
        }
    }

    return static_cast<int32_t>(static_cast<uint32_t>(m >> 32));
}

int32_t MersenneTwister::nextInt(int32_t origin, int32_t bound) {
    const int32_t n = wrapSubtract(bound, origin);

    if (n > 0) {
        return wrapAdd(nextInt(n), origin);
    }

    int32_t value = 0;

    do {
        value = nextInt();
    } while (value < origin || value >= bound);

    return value;
}

int64_t MersenneTwister::nextLong() {
    const int64_t high = static_cast<int64_t>(static_cast<uint64_t>(static_cast<int64_t>(nextInt())) << 32);
    const int64_t low = toUnsignedLong(nextInt());
    return high | low;
}

double MersenneTwister::nextDouble() {
    const int32_t v = nextInt();
    const int32_t w = nextInt();
    return makeDouble(v, w);
}

float MersenneTwister::nextFloat() {
    return static_cast<float>(unsignedShiftRight(nextInt(), 8)) * 0x1.0p-24f;
}

bool MersenneTwister::nextBoolean() {
    int32_t bits = mBooleanSource;

    if (bits == EMPTY_BOOLEAN_SOURCE) {
        bits = nextInt();
        mBooleanSource = UPPER_MASK | unsignedShiftRight(bits, 1);
    } else {
        mBooleanSource = unsignedShiftRight(bits, 1);
    }

    return (bits & 1) == 1;
}

SimpleRandom::SimpleRandom(int64_t seed)
    : mSeeds(0), mProvider(), mGaussianState(0) {
    setSeed(seed);
}

void SimpleRandom::setSeed(int64_t seed) {
    mSeeds = seed;
    mProvider.setSeed(seed);
    mGaussianState = seed;
}

std::shared_ptr<IRandom> SimpleRandom::fork() {
    return std::make_shared<SimpleRandom>(nextLong());
}

std::shared_ptr<IRandom> SimpleRandom::identical() const {
    return std::make_shared<SimpleRandom>(mSeeds);
}

int32_t SimpleRandom::nextInt() {
    return mProvider.nextInt();
}

int32_t SimpleRandom::nextInt(int32_t max) {
    return mProvider.nextInt(wrapAdd(max, 1));
}

int32_t SimpleRandom::nextInt(int32_t min, int32_t max) {
    return mProvider.nextInt(min, wrapAdd(max, 1));
}

int32_t SimpleRandom::nextBoundedInt(int32_t max) {
    return nextInt(max);
}

int32_t SimpleRandom::nextRange(int32_t min, int32_t max) {
    return mProvider.nextInt(min, wrapAdd(max, 1));
}

int64_t SimpleRandom::nextLong() {
    return mProvider.nextLong();
}

bool SimpleRandom::nextBoolean() {
    return mProvider.nextBoolean();
}

float SimpleRandom::nextFloat() {
    return mProvider.nextFloat();
}

double SimpleRandom::nextDouble() {
    return mProvider.nextDouble();
}

double SimpleRandom::nextGaussian() {
    uint64_t z = static_cast<uint64_t>(mGaussianState);
    double values[2] = {0.0, 0.0};

    for (int32_t i = 0; i < 2; ++i) {
        z += 0x9E3779B97F4A7C15ULL;
        uint64_t r = z;
        r = (r ^ (r >> 30)) * 0xBF58476D1CE4E5B9ULL;
        r = (r ^ (r >> 27)) * 0x94D049BB133111EBULL;
        r ^= (r >> 31);
        values[i] = static_cast<double>(r >> 11) * (1.0 / static_cast<double>(1LL << 53));
    }

    mGaussianState = static_cast<int64_t>(z);

    if (values[0] <= 0.0) {
        values[0] = 0x1.0p-53;
    }

    const double sample = std::sqrt(-2.0 * std::log(values[0])) * std::cos(2.0 * MathConstants::PI * values[1]) * 0.33333;

    if (sample > 1.0) {
        return 1.0;
    }

    if (sample < -1.0) {
        return -1.0;
    }

    return sample;
}

int64_t SimpleRandom::getSeed() const {
    return mSeeds;
}
