#include "Level/LightSystem.h"

#include "Block/BlockLightProperties.h"
#include "Core/Math/MathConstants.h"
#include "Level/Dimension.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"

#include <algorithm>
#include <cmath>
#include <deque>

namespace {
    const float SKY_LIGHT_SCALE = 11.0f;
    const float WEATHER_REDUCTION = 5.0f / 16.0f;

    const int32_t NEIGHBOUR_OFFSETS[6][3] = {
            {1,  0,  0},
            {-1, 0,  0},
            {0,  0,  1},
            {0,  0,  -1},
            {0,  -1, 0},
            {0,  1,  0}
    };

    struct LightNode {
        int32_t mX;
        int32_t mY;
        int32_t mZ;
        int mLight;
    };

    float clampFloat(float value, float minimum, float maximum) {
        return std::max(minimum, std::min(maximum, value));
    }

    int attenuation(const BlockState &state) {
        return std::max(1, BlockLightProperties::lightFilter(BlockLightProperties::packed(state)));
    }

    int emission(const BlockState &state) {
        return BlockLightProperties::lightLevel(BlockLightProperties::packed(state));
    }

    class ChunkLightAccess {
    public:
        explicit ChunkLightAccess(LevelChunk &chunk)
                : mChunk(chunk) {
        }

        bool contains(int32_t x, int32_t y, int32_t z) const {
            return x >= 0 && x <= 15 && z >= 0 && z <= 15 && y >= LevelChunk::MIN_Y && y <= LevelChunk::MAX_Y;
        }

        const BlockState &stateAt(int32_t x, int32_t y, int32_t z) const {
            return mChunk.getBlock(x, y, z);
        }

        int lightAt(int32_t x, int32_t y, int32_t z) const {
            return mChunk.getBlockLight(x, y, z);
        }

        void setLight(int32_t x, int32_t y, int32_t z, int value) {
            mChunk.setBlockLight(x, y, z, value);
        }

    private:
        LevelChunk &mChunk;
    };

    class LevelLightAccess {
    public:
        explicit LevelLightAccess(Level &level)
                : mLevel(level) {
        }

        bool contains(int32_t x, int32_t y, int32_t z) const {
            return y >= LevelChunk::MIN_Y && y <= LevelChunk::MAX_Y && mLevel.peekChunkPtr(x >> 4, z >> 4) != nullptr;
        }

        const BlockState &stateAt(int32_t x, int32_t y, int32_t z) const {
            return mLevel.peekChunkPtr(x >> 4, z >> 4)->getBlock(x & 15, y, z & 15);
        }

        int lightAt(int32_t x, int32_t y, int32_t z) const {
            return mLevel.peekChunkPtr(x >> 4, z >> 4)->getBlockLight(x & 15, y, z & 15);
        }

        void setLight(int32_t x, int32_t y, int32_t z, int value) {
            mLevel.peekChunkPtr(x >> 4, z >> 4)->setBlockLight(x & 15, y, z & 15, value);
        }

    private:
        Level &mLevel;
    };

    template<typename Access>
    void removeBlockLight(Access &access, std::deque<LightNode> &removal, std::deque<LightNode> &spread) {
        while (!removal.empty()) {
            const LightNode node = removal.front();
            removal.pop_front();

            for (const auto &offset: NEIGHBOUR_OFFSETS) {
                const int32_t x = node.mX + offset[0];
                const int32_t y = node.mY + offset[1];
                const int32_t z = node.mZ + offset[2];
                if (!access.contains(x, y, z))
                    continue;

                const int current = access.lightAt(x, y, z);
                if (current == 0)
                    continue;

                if (current < node.mLight) {
                    access.setLight(x, y, z, 0);
                    removal.push_back(LightNode{x, y, z, current});

                    const int source = emission(access.stateAt(x, y, z));
                    if (source > 0) {
                        access.setLight(x, y, z, source);
                        spread.push_back(LightNode{x, y, z, source});
                    }
                } else {
                    spread.push_back(LightNode{x, y, z, current});
                }
            }
        }
    }

    template<typename Access>
    void spreadBlockLight(Access &access, std::deque<LightNode> &spread) {
        while (!spread.empty()) {
            const LightNode node = spread.front();
            spread.pop_front();

            const int light = access.lightAt(node.mX, node.mY, node.mZ);
            if (light <= 1)
                continue;

            for (const auto &offset: NEIGHBOUR_OFFSETS) {
                const int32_t x = node.mX + offset[0];
                const int32_t y = node.mY + offset[1];
                const int32_t z = node.mZ + offset[2];
                if (!access.contains(x, y, z))
                    continue;

                const int next = light - attenuation(access.stateAt(x, y, z));
                if (next <= access.lightAt(x, y, z))
                    continue;

                access.setLight(x, y, z, next);
                if (next > 1)
                    spread.push_back(LightNode{x, y, z, next});
            }
        }
    }
}

int64_t LightSystem::packPosition(int32_t x, int32_t y, int32_t z) {
    return (((int64_t) x & 0x3FFFFFF) << 38) | (((int64_t) z & 0x3FFFFFF) << 12) | ((int64_t) (y + 2048) & 0xFFF);
}

float LightSystem::calculateCelestialAngle(int64_t time) {
    const int32_t dayTime = (int32_t) (time % 24000);
    float angle = (float) dayTime / 24000.0f - 0.25f;

    if (angle < 0.0f)
        ++angle;
    if (angle > 1.0f)
        --angle;

    const float smoothed = 1.0f - (float) ((std::cos((double) angle * MathConstants::PI) + 1.0) / 2.0);
    return angle + (smoothed - angle) / 3.0f;
}

int32_t LightSystem::calculateSkyLightSubtracted(const Level &level, bool includeWeather) {
    const float rain = includeWeather && level.isRaining() ? 1.0f : 0.0f;
    const float thunder = includeWeather && level.isThundering() ? 1.0f : 0.0f;

    const float rainFactor = 1.0f - rain * WEATHER_REDUCTION;
    const float thunderFactor = 1.0f - thunder * WEATHER_REDUCTION;

    const float angle = calculateCelestialAngle(level.getTime());
    const float brightness = 0.5f + 2.0f * clampFloat(std::cos(angle * MathConstants::TWO_PI_F), -0.25f, 0.25f);

    return (int32_t) ((1.0f - brightness * rainFactor * thunderFactor) * SKY_LIGHT_SCALE);
}

void LightSystem::updateHeightAt(LevelChunk &chunk, int x, int z) {
    for (int index = LevelChunk::SUB_CHUNK_COUNT - 1; index >= 0; --index) {
        if (chunk.getSubChunk(index).isEmpty())
            continue;

        const int32_t baseY = LevelChunk::MIN_Y + index * 16;
        for (int32_t y = baseY + 15; y >= baseY; --y) {
            const int32_t packed = BlockLightProperties::packed(chunk.getBlock(x, y, z));
            if (BlockLightProperties::lightFilter(packed) > 1 || BlockLightProperties::diffusesSkyLight(packed)) {
                chunk.setHeight(x, z, y + 1);
                return;
            }
        }
    }

    chunk.setHeight(x, z, LevelChunk::MIN_Y);
}

void LightSystem::computeHeightmap(LevelChunk &chunk) {
    for (int x = 0; x < 16; ++x) {
        for (int z = 0; z < 16; ++z)
            updateHeightAt(chunk, x, z);
    }
}

void LightSystem::_computeSkyColumn(LevelChunk &chunk, int x, int z) {
    const int32_t height = chunk.getHeight(x, z);

    for (int32_t y = LevelChunk::MAX_Y; y >= height; --y)
        chunk.setSkyLight(x, y, z, MAX_LIGHT);

    int light = MAX_LIGHT;
    int nextDecrease = 0;

    for (int32_t y = height - 1; y >= LevelChunk::MIN_Y; --y) {
        light = std::max(0, light - nextDecrease);
        chunk.setSkyLight(x, y, z, light);

        if (light == 0) {
            for (int32_t below = y - 1; below >= LevelChunk::MIN_Y; --below)
                chunk.setSkyLight(x, below, z, 0);
            return;
        }

        const int32_t packed = BlockLightProperties::packed(chunk.getBlock(x, y, z));
        if (!BlockLightProperties::isTransparent(packed))
            light = 0;
        else if (BlockLightProperties::diffusesSkyLight(packed))
            nextDecrease += 1;
        else
            nextDecrease += BlockLightProperties::lightFilter(packed);
    }
}

void LightSystem::computeSkyLight(LevelChunk &chunk) {
    if (!Dimension::hasSkyLight(chunk.getDimension()))
        return;

    computeHeightmap(chunk);

    for (int x = 0; x < 16; ++x) {
        for (int z = 0; z < 16; ++z)
            _computeSkyColumn(chunk, x, z);
    }
}

void LightSystem::computeBlockLight(LevelChunk &chunk) {
    ChunkLightAccess access(chunk);
    std::deque<LightNode> spread;

    for (int index = 0; index < LevelChunk::SUB_CHUNK_COUNT; ++index) {
        if (!chunk.getSubChunk(index).hasLightEmitter())
            continue;

        const int32_t baseY = LevelChunk::MIN_Y + index * 16;
        for (int32_t y = baseY; y < baseY + 16; ++y) {
            for (int32_t z = 0; z < 16; ++z) {
                for (int32_t x = 0; x < 16; ++x) {
                    const int source = emission(chunk.getBlock(x, y, z));
                    if (source <= 0)
                        continue;

                    chunk.setBlockLight(x, y, z, source);
                    spread.push_back(LightNode{x, y, z, source});
                }
            }
        }
    }

    spreadBlockLight(access, spread);
}

void LightSystem::onBlockChanged(Level &level, int32_t x, int32_t y, int32_t z) {
    LevelChunk *chunk = level.peekChunkPtr(x >> 4, z >> 4);
    if (chunk == nullptr)
        return;

    const int localX = x & 15;
    const int localZ = z & 15;

    if (chunk->hasHeightmap())
        updateHeightAt(*chunk, localX, localZ);

    if (chunk->hasSkyLight())
        _computeSkyColumn(*chunk, localX, localZ);

    level.addBlockLightUpdate(x, y, z);
}

void LightSystem::updateBlockLight(Level &level, std::unordered_set<int64_t> &pending) {
    if (pending.empty())
        return;

    LevelLightAccess access(level);
    std::deque<LightNode> removal;
    std::deque<LightNode> spread;

    for (const int64_t key: pending) {
        const int32_t x = (int32_t) (key >> 38);
        const int32_t z = (int32_t) (((int64_t) ((uint64_t) key << 26)) >> 38);
        const int32_t y = (int32_t) (key & 0xFFF) - 2048;
        if (!access.contains(x, y, z))
            continue;

        const int oldLevel = access.lightAt(x, y, z);
        const int newLevel = emission(access.stateAt(x, y, z));

        if (newLevel < oldLevel) {
            access.setLight(x, y, z, newLevel);
            removal.push_back(LightNode{x, y, z, oldLevel});
            if (newLevel > 0)
                spread.push_back(LightNode{x, y, z, newLevel});
            continue;
        }

        if (newLevel > oldLevel)
            access.setLight(x, y, z, newLevel);

        spread.push_back(LightNode{x, y, z, newLevel});
        for (const auto &offset: NEIGHBOUR_OFFSETS) {
            const int32_t neighbourX = x + offset[0];
            const int32_t neighbourY = y + offset[1];
            const int32_t neighbourZ = z + offset[2];
            if (access.contains(neighbourX, neighbourY, neighbourZ)
                && access.lightAt(neighbourX, neighbourY, neighbourZ) > 1)
                spread.push_back(LightNode{neighbourX, neighbourY, neighbourZ, 0});
        }
    }

    pending.clear();

    removeBlockLight(access, removal, spread);
    spreadBlockLight(access, spread);
}
