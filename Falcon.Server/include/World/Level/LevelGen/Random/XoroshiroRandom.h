#pragma once

#include "World/Level/LevelGen/Random/IRandom.h"

class XoroshiroPositionalFactory;

// not thread-safe: mutable state per instance
class XoroshiroRandom : public IRandom {
public:
    explicit XoroshiroRandom(int64_t seed);

    ~XoroshiroRandom() override = default;

    // no 64-bit seed: getSeed() returns 0
    static XoroshiroRandom fromState(int64_t lo, int64_t hi);

    XoroshiroPositionalFactory forkPositional();

    std::shared_ptr<IRandom> fork() override;

    // initial state, not a snapshot of the current one
    std::shared_ptr<IRandom> identical() const override;

    int32_t nextInt() override;

    // 0 when max is not positive
    int32_t nextInt(int32_t max) override;

    int32_t nextInt(int32_t min, int32_t max) override;

    int32_t nextBoundedInt(int32_t max) override;

    // lemire's method, like the game
    int32_t nextExclusiveInt(int32_t bound);

    int64_t nextLong() override;

    bool nextBoolean() override;

    float nextFloat() override;

    double nextDouble() override;

    double nextGaussian() override;

    void setSeed(int64_t seed) override;

    int64_t getSeed() const override;

private:
    XoroshiroRandom(int64_t seed, int64_t lo, int64_t hi);

    void _setState(int64_t lo, int64_t hi);

    static int64_t _rotateLeft(int64_t x, int32_t k);

    int64_t mSeed;
    int64_t mInitialLo;
    int64_t mInitialHi;
    int64_t mS0;
    int64_t mS1;
    double mNextGaussian;
    bool mHaveNextGaussian;
};
