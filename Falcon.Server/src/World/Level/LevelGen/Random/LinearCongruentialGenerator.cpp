#include "World/Level/LevelGen/Random/LinearCongruentialGenerator.h"

namespace {

bool isPowerOf2(int64_t value) {
    return (value & -value) == value;
}

int32_t numberOfTrailingZeros(int64_t value) {
    if (value == 0) {
        return 64;
    }

    int32_t count = 0;
    uint64_t bits = static_cast<uint64_t>(value);

    while ((bits & 1ULL) == 0ULL) {
        bits >>= 1;
        ++count;
    }

    return count;
}

}

LinearCongruentialGenerator::LinearCongruentialGenerator(int64_t multiplier, int64_t addend, int64_t modulus)
    : mMultiplier(multiplier),
      mAddend(addend),
      mModulus(modulus),
      mIsPowerOf2(isPowerOf2(modulus)),
      mTrailingZeros(isPowerOf2(modulus) ? numberOfTrailingZeros(modulus) : -1) {
}

const LinearCongruentialGenerator &LinearCongruentialGenerator::java() {
    static const LinearCongruentialGenerator instance(25214903917LL, 11LL, 1LL << 48);
    return instance;
}

const LinearCongruentialGenerator &LinearCongruentialGenerator::skip262() {
    static const LinearCongruentialGenerator instance = java().combine(262);
    return instance;
}

bool LinearCongruentialGenerator::isModPowerOf2() const {
    return mIsPowerOf2;
}

int32_t LinearCongruentialGenerator::getModTrailingZeroes() const {
    return mTrailingZeros;
}

bool LinearCongruentialGenerator::isMultiplicative() const {
    return mAddend == 0;
}

int64_t LinearCongruentialGenerator::nextSeed(int64_t seed) const {
    uint64_t value = static_cast<uint64_t>(seed) * static_cast<uint64_t>(mMultiplier) + static_cast<uint64_t>(mAddend);
    return mod(static_cast<int64_t>(value));
}

int64_t LinearCongruentialGenerator::mod(int64_t n) const {
    if (isModPowerOf2()) {
        return n & (mModulus - 1);
    }

    return static_cast<int64_t>(static_cast<uint64_t>(n) % static_cast<uint64_t>(mModulus));
}

LinearCongruentialGenerator LinearCongruentialGenerator::combine(int64_t steps) const {
    uint64_t multiplier = 1;
    uint64_t addend = 0;

    uint64_t intermediateMultiplier = static_cast<uint64_t>(mMultiplier);
    uint64_t intermediateAddend = static_cast<uint64_t>(mAddend);

    for (uint64_t k = static_cast<uint64_t>(steps); k != 0; k >>= 1) {
        if ((k & 1ULL) != 0ULL) {
            multiplier *= intermediateMultiplier;
            addend = intermediateMultiplier * addend + intermediateAddend;
        }

        intermediateAddend = (intermediateMultiplier + 1ULL) * intermediateAddend;
        intermediateMultiplier *= intermediateMultiplier;
    }

    int64_t finalMultiplier = mod(static_cast<int64_t>(multiplier));
    int64_t finalAddend = mod(static_cast<int64_t>(addend));

    return LinearCongruentialGenerator(finalMultiplier, finalAddend, mModulus);
}

int64_t LinearCongruentialGenerator::getMultiplier() const {
    return mMultiplier;
}

int64_t LinearCongruentialGenerator::getAddend() const {
    return mAddend;
}

int64_t LinearCongruentialGenerator::getModulus() const {
    return mModulus;
}
