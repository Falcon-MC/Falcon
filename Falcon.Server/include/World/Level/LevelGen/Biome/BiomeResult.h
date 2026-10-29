#pragma once

#include <cstdint>

class BiomeResult {
public:
    virtual ~BiomeResult() = default;

    int32_t getBiomeId() const {
        return mBiomeId;
    }

protected:
    explicit BiomeResult(int32_t biomeId) : mBiomeId(biomeId) {
    }

public:
    int32_t mBiomeId;
};
