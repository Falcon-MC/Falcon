#pragma once

#include <cstdint>

class LinearCongruentialGenerator {
public:
    LinearCongruentialGenerator(int64_t multiplier, int64_t addend, int64_t modulus);

    static const LinearCongruentialGenerator &java();

    static const LinearCongruentialGenerator &skip262();

    bool isModPowerOf2() const;

    int32_t getModTrailingZeroes() const;

    bool isMultiplicative() const;

    int64_t nextSeed(int64_t seed) const;

    int64_t mod(int64_t n) const;

    LinearCongruentialGenerator combine(int64_t steps) const;

    int64_t getMultiplier() const;

    int64_t getAddend() const;

    int64_t getModulus() const;

private:
    int64_t mMultiplier;
    int64_t mAddend;
    int64_t mModulus;
    bool mIsPowerOf2;
    int32_t mTrailingZeros;
};
