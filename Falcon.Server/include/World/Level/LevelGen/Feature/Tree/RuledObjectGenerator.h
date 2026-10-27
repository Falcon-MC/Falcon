#pragma once

#include <cstdint>
#include <string>

class RuledObjectGenerator {
public:
    virtual ~RuledObjectGenerator() = default;

    virtual std::string getName() const = 0;

    virtual bool canGenerateAt(int32_t x, int32_t y, int32_t z) const = 0;
};
