#pragma once

#include "World/Level/Dimension/Dimension.h"
#include "World/Level/LevelGen/ChunkGenerator.h"

#include <cstdint>
#include <memory>
#include <string>

class DimensionFactory {
public:
    // values of the level.dat Generator key
    enum class GeneratorType : int32_t {
        Overworld = 1,
        Flat = 2
    };

    static std::unique_ptr<ChunkGenerator> createGenerator(DimensionType dimension, int64_t seed,
                                                           GeneratorType type = GeneratorType::Overworld,
                                                           const std::string &flatWorldLayers = std::string());
};
