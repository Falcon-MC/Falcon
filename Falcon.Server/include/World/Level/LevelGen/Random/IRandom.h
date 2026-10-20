#pragma once

#include <cstdint>
#include <memory>

class IRandom {
public:
    virtual ~IRandom() = default;

    virtual std::shared_ptr<IRandom> fork() = 0;

    virtual std::shared_ptr<IRandom> identical() const = 0;

    virtual int32_t nextInt() = 0;

    virtual int32_t nextInt(int32_t max) = 0;

    virtual int32_t nextInt(int32_t min, int32_t max) = 0;

    virtual int32_t nextBoundedInt(int32_t max) = 0;

    virtual int64_t nextLong() = 0;

    virtual bool nextBoolean() = 0;

    virtual float nextFloat() = 0;

    virtual double nextDouble() = 0;

    virtual double nextGaussian() = 0;

    virtual void setSeed(int64_t seed) = 0;

    virtual int64_t getSeed() const = 0;
};

using IRandomPtr = std::shared_ptr<IRandom>;
