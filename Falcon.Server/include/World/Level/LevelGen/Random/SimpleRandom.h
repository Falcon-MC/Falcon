#pragma once

#include "World/Level/LevelGen/Random/IRandom.h"

#include <array>
#include <vector>

class MersenneTwister {
public:
    static constexpr int32_t N = 624;
    static constexpr int32_t M = 397;

    MersenneTwister();

    void setSeed(int64_t seed);

    void setSeed(const std::vector<int32_t> &seed);

    int32_t nextInt();

    int32_t nextInt(int32_t n);

    int32_t nextInt(int32_t origin, int32_t bound);

    int64_t nextLong();

    double nextDouble();

    float nextFloat();

    bool nextBoolean();

private:
    void _initState();

    void _mixSeedAndState(const std::vector<int32_t> &seed);

    void _mixState();

    static std::vector<int32_t> _expandSeed(int64_t seed, int32_t length);

    std::array<int32_t, N> mMt;
    int32_t mMti;
    int32_t mMixIndex;
    int32_t mBooleanSource;
};

class SimpleRandom : public IRandom {
public:
    explicit SimpleRandom(int64_t seed);

    ~SimpleRandom() override = default;

    std::shared_ptr<IRandom> fork() override;

    std::shared_ptr<IRandom> identical() const override;

    int32_t nextInt() override;

    int32_t nextInt(int32_t max) override;

    int32_t nextInt(int32_t min, int32_t max) override;

    int32_t nextBoundedInt(int32_t max) override;

    int32_t nextRange(int32_t min, int32_t max);

    int64_t nextLong() override;

    bool nextBoolean() override;

    float nextFloat() override;

    double nextDouble() override;

    double nextGaussian() override;

    void setSeed(int64_t seed) override;

    int64_t getSeed() const override;

private:
    int64_t mSeeds;
    MersenneTwister mProvider;
    int64_t mGaussianState;
};
