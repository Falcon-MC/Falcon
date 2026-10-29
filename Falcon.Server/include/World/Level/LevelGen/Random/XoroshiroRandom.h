#pragma once

#include "World/Level/LevelGen/Random/IRandom.h"

class XoroshiroRandom : public IRandom {
public:
    explicit XoroshiroRandom(int64_t seed);

    ~XoroshiroRandom() override = default;

    std::shared_ptr<IRandom> fork() override;

    std::shared_ptr<IRandom> identical() const override;

    int32_t nextInt() override;

    int32_t nextInt(int32_t max) override;

    int32_t nextInt(int32_t min, int32_t max) override;

    int32_t nextBoundedInt(int32_t max) override;

    int64_t nextLong() override;

    bool nextBoolean() override;

    float nextFloat() override;

    double nextDouble() override;

    double nextGaussian() override;

    void setSeed(int64_t seed) override;

    int64_t getSeed() const override;

private:
    static int64_t _rotateLeft(int64_t x, int32_t k);

    int64_t mSeed;
    int64_t mS0;
    int64_t mS1;
};
