#include "World/Level/LevelGen/Structure/EndCityFeature.h"

#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/ChunkGenerator.h"

#include <algorithm>
#include <array>

namespace {
    const int32_t SPACING = 20;
    const int32_t SEPARATION = 11;
    const int32_t SALT = 10387313;
    // farther than 64 chunks from the main island, on chunk coordinates
    const int64_t MIN_DISTANCE_SQUARED = 4097;
    const int32_t MIN_BASE_Y = 60;

    const EndCityFeature gEndCity;
}

EndCityFeature::EndCityFeature() : StructureFeature("end_city", SPACING, SEPARATION, SALT, Spread::Triangular) {}

std::mt19937 EndCityFeature::createCityRandom(int32_t chunkX, int32_t chunkZ) {
    return std::mt19937((uint32_t) chunkX + (uint32_t) SALT * (uint32_t) chunkZ);
}

int32_t EndCityFeature::getBaseY(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ,
                                 int32_t quarterTurns) {
    // the far corner sits five blocks from the centre column, turned with the city
    constexpr std::array<int32_t, 4> FAR_X = {12, 2, 2, 12};
    constexpr std::array<int32_t, 4> FAR_Z = {12, 12, 2, 2};
    const std::unique_ptr<LevelChunk> terrain = generator.generateTerrain(chunkX, chunkZ);
    const int32_t farX = FAR_X[quarterTurns & 3];
    const int32_t farZ = FAR_Z[quarterTurns & 3];
    return std::min({ChunkGenerator::solidHeight(*terrain, 7, 7), ChunkGenerator::solidHeight(*terrain, 7, farZ),
                     ChunkGenerator::solidHeight(*terrain, farX, 7),
                     ChunkGenerator::solidHeight(*terrain, farX, farZ)});
}

bool EndCityFeature::_isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const {
    if ((int64_t) chunkX * chunkX + (int64_t) chunkZ * chunkZ < MIN_DISTANCE_SQUARED)
        return false;
    std::mt19937 random = createCityRandom(chunkX, chunkZ);
    return getBaseY(generator, chunkX, chunkZ, (int32_t) (random() & 3)) >= MIN_BASE_Y;
}
