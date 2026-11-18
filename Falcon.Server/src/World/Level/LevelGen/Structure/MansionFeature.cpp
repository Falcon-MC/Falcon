#include "World/Level/LevelGen/Structure/MansionFeature.h"

#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Structure/StructureTerrain.h"

#include <algorithm>
#include <array>

namespace {
    const int32_t SPACING = 80;
    const int32_t SEPARATION = 20;
    const int32_t SALT = 10387319;
    const int32_t BIOME_RADIUS = 32;
    const int32_t ROOFED_FOREST_MUTATED = 157;

    const MansionFeature gMansion;

    bool isMansionBiome(int32_t biomeId) {
        switch (biomeId) {
            case BiomeIds::ROOFED_FOREST:
            case ROOFED_FOREST_MUTATED:
            case BiomeIds::PALE_GARDEN:
            case BiomeIds::LUSH_CAVES:
            case BiomeIds::DRIPSTONE_CAVES:
            case BiomeIds::SULFUR_CAVES:
            case BiomeIds::ICE_CAVES:
                return true;
            default:
                return false;
        }
    }
}

MansionFeature::MansionFeature() : StructureFeature("mansion", SPACING, SEPARATION, SALT, Spread::Triangular) {}

int32_t MansionFeature::getBaseY(StructureTerrain &terrain, int32_t chunkX, int32_t chunkZ, int32_t quarterTurns) {
    constexpr std::array<int32_t, 4> FAR_X = {12, 12, 2, 2};
    constexpr std::array<int32_t, 4> FAR_Z = {12, 2, 2, 12};
    const int32_t x = chunkX * 16;
    const int32_t z = chunkZ * 16;
    const int32_t farX = x + FAR_X[quarterTurns & 3];
    const int32_t farZ = z + FAR_Z[quarterTurns & 3];
    return 1 + std::min({terrain.getSolidHeight(x + 7, z + 7), terrain.getSolidHeight(x + 7, farZ),
                         terrain.getSolidHeight(farX, z + 7), terrain.getSolidHeight(farX, farZ)});
}

bool MansionFeature::_isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const {
    const int32_t x = chunkX * 16 + 8;
    const int32_t z = chunkZ * 16 + 8;
    const int32_t y = generator.getPreliminarySurfaceLevel(chunkX * 4, chunkZ * 4);
    return _hasOnlyBiomes(generator, x, y, z, BIOME_RADIUS, isMansionBiome);
}
