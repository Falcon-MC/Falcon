#pragma once

#include <cstdint>

class BiomeSource {
public:
    virtual ~BiomeSource() = default;

    virtual int32_t pickBiomeId(int32_t x, int32_t y, int32_t z) const = 0;
};
