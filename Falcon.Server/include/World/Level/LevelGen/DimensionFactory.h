#pragma once

#include "World/Level/Dimension/Dimension.h"
#include "World/Level/LevelGen/ChunkGenerator.h"

#include <cstdint>
#include <memory>

class DimensionFactory {
public:
    static std::unique_ptr<ChunkGenerator> createGenerator(DimensionType dimension, int64_t seed);
};
