#include "World/Level/Dimension/EndGatewayExit.h"

#include "World/Level/Block/BlockShape.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Block/BlockSupport.h"

#include <algorithm>
#include <cmath>

namespace {

    const char *const AIR = "minecraft:air";
    const char *const BEDROCK = "minecraft:bedrock";
    const char *const END_STONE = "minecraft:end_stone";
    const char *const END_GATEWAY = "minecraft:end_gateway";

    const uint64_t FNV_OFFSET = 0xCBF29CE484222325ULL;
    const uint64_t FNV_PRIME = 0x100000001B3ULL;
    const uint64_t HASH_COMBINE_GOLDEN = 0x9E3779B9ULL;

    bool isNamed(const BlockState *state, const char *name) {
        return state != nullptr && state->mName == name;
    }

    bool isAir(const BlockState *state) {
        return state == nullptr || state->mName == AIR;
    }

    bool isSolid(const BlockState *state) {
        return state != nullptr && BlockSupport::isSolid(*state);
    }

    bool isSolidFull(const BlockState *state) {
        return isSolid(state) && BlockShape::isFullCube(*state);
    }

    bool isPortal(const BlockState *state) {
        return isNamed(state, END_GATEWAY) || isNamed(state, "minecraft:end_portal")
               || isNamed(state, "minecraft:portal");
    }

    /**
     * Moves y down to the first air of the column, then on to the first solid block under it (sub_1432DD020). False
     * when the column has no air or nothing solid under it.
     */
    bool findColumnTop(const EndGatewayExit::BlockReader &reader, int32_t x, int32_t &y, int32_t z, int32_t minY) {
        for (;;) {
            const BlockState *state = reader(x, y, z);
            const int32_t current = y--;
            if (isAir(state))
                break;

            if (current <= minY)
                return false;
        }

        for (;;) {
            if (isSolid(reader(x, y, z)))
                return true;

            const int32_t current = y--;
            if (current <= minY)
                return false;
        }
    }

    // an end stone top the game lands entities on: open above and with no air beside it
    bool isEnclosedTop(const EndGatewayExit::BlockReader &reader, int32_t x, int32_t y, int32_t z) {
        static const int32_t SIDES[8][2] = {{1, 0}, {-1, 0}, {0, -1}, {0, 1}, {1, -1}, {1, 1}, {-1, -1}, {-1, 1}};

        if (isSolid(reader(x, y + 1, z)))
            return false;

        for (const int32_t *side: SIDES) {
            if (isAir(reader(x + side[0], y, z + side[1])))
                return false;
        }

        return true;
    }

    uint64_t hashInt(int32_t value) {
        const uint32_t bits = (uint32_t) value;
        uint64_t hash = FNV_OFFSET;
        for (int32_t shift = 0; shift < 32; shift += 8)
            hash = (hash ^ ((bits >> shift) & 0xFFu)) * FNV_PRIME;
        return hash;
    }

    void hashCombine(uint64_t &seed, uint64_t value) {
        seed ^= value + HASH_COMBINE_GOLDEN + (seed << 6) + (seed >> 2);
    }

}

Vector3i EndGatewayExit::findRoughExit(const Vector3i &gateway, const TerrainCheck &hasTerrain) {
    const float x = (float) gateway.x;
    const float z = (float) gateway.z;
    const float length = std::sqrt(z * z + x * x);
    const bool flat = length < 0.0001f;
    const float directionX = flat ? 0.0f : x / length;
    const float directionZ = flat ? 0.0f : z / length;

    float pointX = EXIT_DISTANCE * directionX;
    float pointZ = EXIT_DISTANCE * directionZ;
    const float stepX = SEARCH_STEP * directionX;
    const float stepZ = SEARCH_STEP * directionZ;

    const auto terrainAt = [&]() {
        return hasTerrain((int32_t) std::floor(pointX) >> 4, (int32_t) std::floor(pointZ) >> 4);
    };

    if (terrainAt()) {
        for (int32_t step = 0; step < SEARCH_STEPS; step++) {
            pointX -= stepX;
            pointZ -= stepZ;
            if (!terrainAt())
                break;
        }
    }

    if (!terrainAt()) {
        for (int32_t step = 0; step < SEARCH_STEPS; step++) {
            pointX += stepX;
            pointZ += stepZ;
            if (terrainAt())
                break;
        }
    }

    return Vector3i((int32_t) std::floor(pointX), 0, (int32_t) std::floor(pointZ));
}

Vector3i EndGatewayExit::findSpawn(const BlockReader &reader, const Vector3i &around, bool endStoneOnly,
                                   int32_t chunkRadius, bool fallback, int32_t minY, int32_t maxY) {
    const int32_t reach = 16 * chunkRadius;
    const float centerX = (float) around.x + 0.5f;
    const float centerY = (float) around.y + 0.5f;
    const float centerZ = (float) around.z + 0.5f;

    bool found = false;
    bool foundValid = false;
    float closest = 0.0f;
    float closestValid = 0.0f;
    Vector3i nearest;
    Vector3i nearestValid;

    for (int32_t x = around.x - reach; x <= around.x + reach; x++) {
        for (int32_t z = around.z - reach; z <= around.z + reach; z++) {
            int32_t y = std::min(SPAWN_SEARCH_TOP_Y, maxY);
            if (!findColumnTop(reader, x, y, z, minY))
                continue;

            const BlockState *top = reader(x, y, z);
            bool valid = false;
            if (isNamed(top, BEDROCK) && isNamed(reader(x, y - 2, z), END_GATEWAY)) {
                // the frame of a gateway is skipped down to the land under it
                y -= 2;
                if (!findColumnTop(reader, x, y, z, minY) || isSolidFull(reader(x, y + 1, z))
                    || isSolidFull(reader(x, y + 2, z)))
                    continue;

                valid = isEnclosedTop(reader, x, y, z);
            } else if (!endStoneOnly) {
                if (isNamed(top, BEDROCK) || isSolidFull(reader(x, y + 1, z)) || isSolidFull(reader(x, y + 2, z)))
                    continue;
            } else {
                if (!isNamed(top, END_STONE))
                    continue;

                valid = isEnclosedTop(reader, x, y, z);
            }

            const float dx = (float) x + 0.5f - centerX;
            const float dy = (float) y + 0.5f - centerY;
            const float dz = (float) z + 0.5f - centerZ;
            const float distance = dz * dz + (dy * dy + dx * dx);
            if (!found || closest > distance) {
                nearest = Vector3i(x, y, z);
                closest = distance;
                found = true;
            }

            if (valid && (!foundValid || closestValid > distance)) {
                nearestValid = Vector3i(x, y, z);
                closestValid = distance;
                foundValid = true;
            }
        }
    }

    if (foundValid)
        return nearestValid;

    if (!fallback)
        return found ? nearest : Vector3i(0, 0, 0);

    if (found)
        return nearest;

    // climb the column of around to the top of its first solid block
    Vector3i column(around.x, std::max(around.y, minY), around.z);
    while (!isSolidFull(reader(column.x, column.y, column.z))) {
        if (column.y++ >= maxY)
            return around;
    }

    while (!isAir(reader(column.x, column.y, column.z))) {
        if (column.y++ >= maxY)
            return around;
    }

    column.y--;
    return column;
}

Vector3i EndGatewayExit::findTallestBlock(const BlockReader &reader, const Vector3i &around, int32_t radius,
                                          bool allowBedrock, bool skipCenter, int32_t maxY) {
    Vector3i tallest(0, 0, 0);

    for (int32_t dx = -radius; dx <= radius; dx++) {
        for (int32_t dz = -radius; dz <= radius; dz++) {
            if (skipCenter && dx == 0 && dz == 0)
                continue;

            for (int32_t y = maxY; --y > tallest.y;) {
                const BlockState *state = reader(around.x + dx, y, around.z + dz);
                if (isSolidFull(state) && (allowBedrock || !isNamed(state, BEDROCK))) {
                    tallest = Vector3i(around.x + dx, y, around.z + dz);
                    break;
                }
            }
        }
    }

    return tallest;
}

bool EndGatewayExit::isExitBlocked(const BlockReader &reader, const Vector3i &exit) {
    const BlockState *feet = reader(exit.x, exit.y, exit.z);
    const BlockState *head = reader(exit.x, exit.y + 1, exit.z);
    return isSolidFull(feet) || isPortal(feet) || isSolidFull(head) || isPortal(head)
           || !isSolidFull(reader(exit.x, exit.y - 1, exit.z));
}

uint32_t EndGatewayExit::islandSeed(const Vector3i &position) {
    uint64_t seed = 0;
    hashCombine(seed, hashInt(position.x));
    hashCombine(seed, hashInt(position.y));
    hashCombine(seed, hashInt(position.z));
    return (uint32_t) seed;
}

bool EndGatewayExit::isZero(const Vector3i &position) {
    return position.x == 0 && position.y == 0 && position.z == 0;
}
