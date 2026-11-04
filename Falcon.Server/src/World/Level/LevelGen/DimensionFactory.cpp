#include "World/Level/LevelGen/DimensionFactory.h"

#include "World/Level/LevelGen/End/TheEndGenerator.h"
#include "World/Level/LevelGen/Flat/FlatWorldGenerator.h"
#include "World/Level/LevelGen/Nether/NetherGenerator.h"
#include "World/Level/LevelGen/Overworld/OverworldGenerator.h"

std::unique_ptr<ChunkGenerator> DimensionFactory::createGenerator(DimensionType dimension, int64_t seed,
                                                                  GeneratorType type,
                                                                  const std::string &flatWorldLayers) {
    switch (dimension) {
        case DimensionType::Nether:
            return std::unique_ptr<ChunkGenerator>(new NetherGenerator(seed));
        case DimensionType::TheEnd:
            return std::unique_ptr<ChunkGenerator>(new TheEndGenerator(seed));
        default:
            if (type == GeneratorType::Flat)
                return std::unique_ptr<ChunkGenerator>(new FlatWorldGenerator(seed, flatWorldLayers));
            return std::unique_ptr<ChunkGenerator>(new OverworldGenerator(seed));
    }
}
