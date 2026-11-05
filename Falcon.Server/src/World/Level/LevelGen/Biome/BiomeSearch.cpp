#include "World/Level/LevelGen/Biome/BiomeSearch.h"

#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Dimension/Dimension.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/ChunkGenerator.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
    const int32_t COARSE_RANGE = 4000;
    const int32_t COARSE_STEP = 64;
    const int32_t FINE_RANGE = 64;
    const int32_t FINE_STEP = 16;
    const int32_t OPENING_RANGE = 16;
    const int32_t FLOOR_OFFSET = 5;

    // cells are visited in cubes of 16 per side, the cube holding the centre first
    const int32_t GROUP_SIZE = 16;

    int32_t clampToRange(int32_t value, int32_t low, int32_t high) {
        int32_t result = low;
        if (value > low)
            result = value;
        if (value > high)
            result = high;

        return result;
    }

    int32_t middleY(int32_t minY, int32_t topY) {
        return minY + (topY - minY - 4) / 2 + FLOOR_OFFSET;
    }

    int32_t cellCount(int32_t from, int32_t to, int32_t step) {
        return (int32_t) std::ceil((float) (to - from) * (float) (1.0 / (double) (float) step));
    }

    bool isOnShell(int32_t x, int32_t y, int32_t z, const Vector3i &center, int32_t shell) {
        return std::abs(x - center.x) == shell || std::abs(y - center.y) == shell || std::abs(z - center.z) == shell;
    }
}

BiomeSearch::Result BiomeSearch::findNearest(const ChunkGenerator &generator, const Vector3i &origin,
                                             int32_t biomeId) {
    const DimensionType dimension = generator.getDimensionType();
    const int32_t minY = Dimension::getMinY(dimension);
    // the game's height range ends one block above the highest y
    const int32_t topY = Dimension::getMaxY(dimension) + 1;

    Result result;
    Vector3i coarse;
    if (!_search(generator, biomeId, Vector3i(origin.x, middleY(minY, topY), origin.z),
                 Vector3i(origin.x - COARSE_RANGE, minY + FLOOR_OFFSET, origin.z - COARSE_RANGE),
                 Vector3i(origin.x + COARSE_RANGE, topY, origin.z + COARSE_RANGE), COARSE_STEP, coarse))
        return result;

    // the game reads a loaded column from its chunk instead; the noise terrain keeps this off the level
    const int32_t surfaceY = generator.getPreliminaryHeight(coarse.x, coarse.z);
    if (generator.pickBiome(coarse.x, surfaceY, coarse.z) == biomeId) {
        result.mFound = true;
        result.mPosition = Vector3i(coarse.x, surfaceY, coarse.z);
        return result;
    }

    Vector3i fine;
    if (!_search(generator, biomeId, Vector3i(coarse.x, middleY(minY, surfaceY), coarse.z),
                 Vector3i(coarse.x - FINE_RANGE, minY + FLOOR_OFFSET, coarse.z - FINE_RANGE),
                 Vector3i(coarse.x + FINE_RANGE, surfaceY, coarse.z + FINE_RANGE), FINE_STEP, fine))
        return result;

    result.mFound = true;
    result.mNeedsOpening = true;
    result.mPosition = fine;
    return result;
}

Vector3i BiomeSearch::findOpening(Level &level, const Vector3i &position, int32_t biomeId) {
    // near the floor the lowest levels are dropped so the search stays above it
    const int32_t belowFloor = std::min(0, position.y - level.getMinY() - 1 - OPENING_RANGE);
    const int32_t lowestOffset = -OPENING_RANGE - belowFloor;

    for (int32_t offsetY = OPENING_RANGE; offsetY > lowestOffset; offsetY -= 2) {
        const int32_t y = position.y + offsetY;
        for (int32_t offsetX = -OPENING_RANGE; offsetX <= OPENING_RANGE; offsetX++) {
            const int32_t x = position.x + offsetX;
            for (int32_t offsetZ = -OPENING_RANGE; offsetZ <= OPENING_RANGE; offsetZ++) {
                const int32_t z = position.z + offsetZ;
                if (!_isBiomeAt(level, x, y, z, biomeId) && !_isBiomeAt(level, x, y + 1, z, biomeId))
                    continue;

                if (_isAirAt(level, x, y, z) && _isAirAt(level, x, y + 1, z))
                    return Vector3i(x, y, z);
            }
        }
    }

    return position;
}

void BiomeSearch::loadOpeningArea(Level &level, const Vector3i &position) {
    for (int32_t chunkX = (position.x - OPENING_RANGE) >> 4; chunkX <= (position.x + OPENING_RANGE) >> 4; chunkX++) {
        for (int32_t chunkZ = (position.z - OPENING_RANGE) >> 4; chunkZ <= (position.z + OPENING_RANGE) >> 4; chunkZ++)
            level.getChunk(chunkX, chunkZ);
    }
}

bool BiomeSearch::_search(const ChunkGenerator &generator, int32_t biomeId, const Vector3i &center, Vector3i min,
                          Vector3i max, int32_t step, Vector3i &found) {
    const int32_t countX = cellCount(min.x, max.x, step);
    const int32_t countY = cellCount(min.y, max.y, step);
    const int32_t countZ = cellCount(min.z, max.z, step);
    max = Vector3i(min.x + countX * step, min.y + countY * step, min.z + countZ * step);

    const Vector3i centerCell(clampToRange((center.x - min.x) / step, 0, countX),
                              clampToRange((center.y - min.y) / step, 0, countY),
                              clampToRange((center.z - min.z) / step, 0, countZ));

    const int32_t groupsX = (int32_t) std::ceil((float) countX * 0.0625f);
    const int32_t groupsY = (int32_t) std::ceil((float) countY * 0.0625f);
    const int32_t groupsZ = (int32_t) std::ceil((float) countZ * 0.0625f);
    const Vector3i centerGroup(centerCell.x / GROUP_SIZE, centerCell.y / GROUP_SIZE, centerCell.z / GROUP_SIZE);
    const int32_t shells = std::max(groupsX, std::max(groupsY, groupsZ));
    const int32_t groupSpan = GROUP_SIZE * step;

    for (int32_t shell = 0; shell < shells; shell++) {
        for (int32_t x = centerGroup.x - shell; x <= centerGroup.x + shell; x++) {
            if (x < 0 || x >= groupsX)
                continue;

            for (int32_t y = centerGroup.y - shell; y <= centerGroup.y + shell; y++) {
                if (y < 0 || y >= groupsY)
                    continue;

                for (int32_t z = centerGroup.z - shell; z <= centerGroup.z + shell; z++) {
                    if (z < 0 || z >= groupsZ || !isOnShell(x, y, z, centerGroup, shell))
                        continue;

                    const Vector3i groupMin(clampToRange(min.x + groupSpan * x, min.x, max.x),
                                            clampToRange(min.y + groupSpan * y, min.y, max.y),
                                            clampToRange(min.z + groupSpan * z, min.z, max.z));
                    const Vector3i groupMax(clampToRange(min.x + groupSpan * (x + 1), min.x, max.x),
                                            clampToRange(min.y + groupSpan * (y + 1), min.y, max.y),
                                            clampToRange(min.z + groupSpan * (z + 1), min.z, max.z));
                    if (_searchGroup(generator, biomeId, center, groupMin, groupMax, step, found))
                        return true;
                }
            }
        }
    }

    return false;
}

bool BiomeSearch::_searchGroup(const ChunkGenerator &generator, int32_t biomeId, const Vector3i &center,
                               const Vector3i &min, const Vector3i &max, int32_t step, Vector3i &found) {
    const int32_t countX = (max.x - min.x) / step;
    const int32_t countY = (max.y - min.y) / step;
    const int32_t countZ = (max.z - min.z) / step;
    const Vector3i centerCell((clampToRange(center.x, min.x, max.x) - min.x) / step,
                              (clampToRange(center.y, min.y, max.y) - min.y) / step,
                              (clampToRange(center.z, min.z, max.z) - min.z) / step);
    const int32_t shells = std::max(countX - 1, std::max(countY - 1, countZ - 1));

    // the game fills the whole group from the biome source first (vfunc +0x20, sub_147186360); each
    // cell lies on one shell only, so sampling it when it is visited gives the same answer for less work
    for (int32_t shell = 0; shell <= shells; shell++) {
        for (int32_t x = centerCell.x - shell; x <= centerCell.x + shell; x++) {
            if (x < 0 || x >= countX)
                continue;

            for (int32_t y = centerCell.y - shell; y <= centerCell.y + shell; y++) {
                if (y < 0 || y >= countY)
                    continue;

                for (int32_t z = centerCell.z - shell; z <= centerCell.z + shell; z++) {
                    if (z < 0 || z >= countZ || !isOnShell(x, y, z, centerCell, shell))
                        continue;

                    const Vector3i position(min.x + step * x, min.y + step * y, min.z + step * z);
                    if (generator.pickBiome(position.x, position.y, position.z) == biomeId) {
                        found = position;
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

bool BiomeSearch::_isBiomeAt(Level &level, int32_t x, int32_t y, int32_t z, int32_t biomeId) {
    if (y < level.getMinY() || y > level.getMaxY())
        return false;

    // a column that is not loaded is unknown, never a match
    const LevelChunk *chunk = level.peekChunkPtr(x >> 4, z >> 4);
    return chunk != nullptr && (int32_t) chunk->getBiomeAt(x & 15, y, z & 15) == biomeId;
}

bool BiomeSearch::_isAirAt(Level &level, int32_t x, int32_t y, int32_t z) {
    const BlockState *state = level.peekBlockPtr(x, y, z);
    return state != nullptr && state->getHash() == level.getAirHash();
}
