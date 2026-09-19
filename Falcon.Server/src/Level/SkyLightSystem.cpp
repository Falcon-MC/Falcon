#include "Level/SkyLightSystem.h"

#include "Block/Block.h"
#include "Block/BlockData.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Core/Math/MathConstants.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"

#include <algorithm>
#include <cmath>
#include <deque>

namespace {
    const float SKY_LIGHT_SCALE = 11.0f;
    const float WEATHER_REDUCTION = 5.0f / 16.0f;

    struct LightNode {
        int32_t mX;
        int32_t mY;
        int32_t mZ;
        int mLight;
    };

    bool blocksSkyLight(const BlockState &state) {
        const Block *block = VanillaBlocks::fromIdentifier(state.mName);
        if (block == nullptr)
            return false;

        const BlockData *data = block->getData();
        if (data == nullptr)
            return false;

        return !data->mTransparent && data->mSolid;
    }

    float clampFloat(float value, float minimum, float maximum) {
        return std::max(minimum, std::min(maximum, value));
    }

    const int32_t NEIGHBOUR_OFFSETS[6][3] = {
            {1,  0,  0},
            {-1, 0,  0},
            {0,  0,  1},
            {0,  0,  -1},
            {0,  -1, 0},
            {0,  1,  0}
    };

    bool isInsideChunk(int32_t x, int32_t y, int32_t z) {
        return x >= 0 && x <= 15 && z >= 0 && z <= 15
               && y >= LevelChunk::MIN_Y && y <= LevelChunk::MAX_Y;
    }

    void propagate(LevelChunk &chunk, std::deque<LightNode> &pending) {
        while (!pending.empty()) {
            const LightNode node = pending.front();
            pending.pop_front();

            const int spread = node.mLight - 1;
            if (spread <= 0)
                continue;

            for (const auto &offset: NEIGHBOUR_OFFSETS) {
                const int32_t nextX = node.mX + offset[0];
                const int32_t nextY = node.mY + offset[1];
                const int32_t nextZ = node.mZ + offset[2];

                if (!isInsideChunk(nextX, nextY, nextZ))
                    continue;
                if (blocksSkyLight(chunk.getBlock((int) nextX, nextY, (int) nextZ)))
                    continue;
                if (chunk.getSkyLight((int) nextX, nextY, (int) nextZ) >= spread)
                    continue;

                chunk.setSkyLight((int) nextX, nextY, (int) nextZ, spread);
                pending.push_back(LightNode{nextX, nextY, nextZ, spread});
            }
        }
    }

    // Darkens everything that was lit by the removed source, collecting the brighter cells at the
    // edge of that region so they can light the hole back up afterwards.
    void removeLight(LevelChunk &chunk, std::deque<LightNode> &removal, std::deque<LightNode> &pending) {
        while (!removal.empty()) {
            const LightNode node = removal.front();
            removal.pop_front();

            for (const auto &offset: NEIGHBOUR_OFFSETS) {
                const int32_t nextX = node.mX + offset[0];
                const int32_t nextY = node.mY + offset[1];
                const int32_t nextZ = node.mZ + offset[2];

                if (!isInsideChunk(nextX, nextY, nextZ))
                    continue;

                const int neighbourLight = chunk.getSkyLight((int) nextX, nextY, (int) nextZ);
                if (neighbourLight == 0)
                    continue;

                if (neighbourLight < node.mLight) {
                    chunk.setSkyLight((int) nextX, nextY, (int) nextZ, 0);
                    removal.push_back(LightNode{nextX, nextY, nextZ, neighbourLight});
                } else {
                    pending.push_back(LightNode{nextX, nextY, nextZ, neighbourLight});
                }
            }
        }
    }
}

float SkyLightSystem::calculateCelestialAngle(int64_t time) {
    const int32_t dayTime = (int32_t) (time % 24000);
    float angle = (float) dayTime / 24000.0f - 0.25f;

    if (angle < 0.0f)
        ++angle;
    if (angle > 1.0f)
        --angle;

    const float smoothed = 1.0f - (float) ((std::cos((double) angle * MathConstants::PI) + 1.0) / 2.0);
    return angle + (smoothed - angle) / 3.0f;
}

int32_t SkyLightSystem::calculateSkyLightSubtracted(const Level &level) {
    const float rain = level.isRaining() ? 1.0f : 0.0f;
    const float thunder = level.isThundering() ? 1.0f : 0.0f;

    const float rainFactor = 1.0f - rain * WEATHER_REDUCTION;
    const float thunderFactor = 1.0f - thunder * WEATHER_REDUCTION;

    const float angle = calculateCelestialAngle(level.getTime());
    const float brightness = 0.5f + 2.0f * clampFloat(std::cos(angle * MathConstants::TWO_PI_F), -0.25f, 0.25f);

    return (int32_t) ((1.0f - brightness * rainFactor * thunderFactor) * SKY_LIGHT_SCALE);
}

void SkyLightSystem::updateHeightAt(LevelChunk &chunk, int x, int z) {
    int32_t height = LevelChunk::MIN_Y;

    for (int index = LevelChunk::SUB_CHUNK_COUNT - 1; index >= 0; --index) {
        if (chunk.getSubChunk(index).isEmpty())
            continue;

        const int32_t baseY = LevelChunk::MIN_Y + index * 16;
        bool found = false;

        for (int32_t offset = 15; offset >= 0; --offset) {
            const int32_t y = baseY + offset;
            if (blocksSkyLight(chunk.getBlock(x, y, z))) {
                height = y + 1;
                found = true;
                break;
            }
        }

        if (found)
            break;
    }

    chunk.setHeight(x, z, height);
}

void SkyLightSystem::computeHeightmap(LevelChunk &chunk) {
    for (int x = 0; x < 16; ++x) {
        for (int z = 0; z < 16; ++z)
            updateHeightAt(chunk, x, z);
    }
}

void SkyLightSystem::computeChunk(LevelChunk &chunk) {
    chunk.clearSkyLightOnly();

    std::deque<LightNode> pending;

    for (int x = 0; x < 16; ++x) {
        for (int z = 0; z < 16; ++z) {
            const int32_t height = chunk.getHeight(x, z);

            for (int32_t y = LevelChunk::MAX_Y; y >= height; --y)
                chunk.setSkyLight(x, y, z, MAX_LIGHT);

            if (height > LevelChunk::MIN_Y)
                pending.push_back(LightNode{x, height, z, MAX_LIGHT});
        }
    }

    propagate(chunk, pending);
}

void SkyLightSystem::onBlockChanged(Level &level, int32_t x, int32_t y, int32_t z) {
    LevelChunk *chunk = level.peekChunkPtr(x >> 4, z >> 4);
    if (chunk == nullptr)
        return;

    const int localX = x & 15;
    const int localZ = z & 15;

    if (!chunk->hasHeightmap())
        return;

    const int32_t oldHeight = chunk->getHeight(localX, localZ);
    updateHeightAt(*chunk, localX, localZ);

    // Both maps are built on demand, so an unlit chunk needs no incremental work here.
    if (!chunk->hasSkyLight())
        return;

    const int32_t newHeight = chunk->getHeight(localX, localZ);

    std::deque<LightNode> pending;
    std::deque<LightNode> removal;

    if (newHeight < oldHeight) {
        for (int32_t columnY = oldHeight - 1; columnY >= newHeight; --columnY) {
            chunk->setSkyLight(localX, columnY, localZ, MAX_LIGHT);
            pending.push_back(LightNode{localX, columnY, localZ, MAX_LIGHT});
        }
    } else if (newHeight > oldHeight) {
        for (int32_t columnY = newHeight - 1; columnY >= oldHeight; --columnY) {
            const int previous = chunk->getSkyLight(localX, columnY, localZ);
            if (previous == 0)
                continue;

            chunk->setSkyLight(localX, columnY, localZ, 0);
            removal.push_back(LightNode{localX, columnY, localZ, previous});
        }
    }

    if (blocksSkyLight(chunk->getBlock(localX, y, localZ))) {
        const int previous = chunk->getSkyLight(localX, y, localZ);
        if (previous > 0) {
            chunk->setSkyLight(localX, y, localZ, 0);
            removal.push_back(LightNode{localX, y, localZ, previous});
        }
    } else {
        // The cell opened up, so let whatever surrounds it flow back in.
        for (const auto &offset: NEIGHBOUR_OFFSETS) {
            const int32_t nextX = localX + offset[0];
            const int32_t nextY = y + offset[1];
            const int32_t nextZ = localZ + offset[2];

            if (!isInsideChunk(nextX, nextY, nextZ))
                continue;

            const int neighbourLight = chunk->getSkyLight((int) nextX, nextY, (int) nextZ);
            if (neighbourLight > 0)
                pending.push_back(LightNode{nextX, nextY, nextZ, neighbourLight});
        }
    }

    removeLight(*chunk, removal, pending);
    propagate(*chunk, pending);
}
