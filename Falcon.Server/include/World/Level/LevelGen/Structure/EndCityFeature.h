#pragma once

#include "World/Level/LevelGen/Structure/StructureFeature.h"

#include <random>

class EndCityFeature : public StructureFeature {
public:
    EndCityFeature();

    DimensionType getDimension() const override {
        return DimensionType::TheEnd;
    }

    // the start draws from its own twister, seeded with the chunk alone: the world seed plays no part
    static std::mt19937 createCityRandom(int32_t chunkX, int32_t chunkZ);

    // the lowest island top of the four columns the city stands on, 0 over the void
    static int32_t getBaseY(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ, int32_t quarterTurns);

protected:
    bool _isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const override;
};
