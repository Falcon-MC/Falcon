#include "World/Level/Map/MapRenderer.h"

#include "World/Level/Block/BlockState.h"
#include "World/Level/Map/MapItemSavedData.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace {
    struct ColorCount {
        float mColor[4];
        const BlockState *mBlock;
        int32_t mCount;
    };

    const float BRIGHTNESS[3] = {0.7058824f, 0.8627451f, 1.0f};
    const float NETHER_STONE[4] = {0.43921569f, 0.43921569f, 0.43921569f, 1.0f};
    const float NETHER_DIRT[4] = {0.59215689f, 0.42745098f, 0.30196080f, 1.0f};

    // the game compares colours once quantised to bytes, and its empty colour quantises to zeros
    bool sameColor(const float left[4], const float right[4]) {
        for (int channel = 0; channel < 4; ++channel) {
            if ((uint8_t) (int32_t) (left[channel] * 255.0f) != (uint8_t) (int32_t) (right[channel] * 255.0f))
                return false;
        }
        return true;
    }

    bool isNone(const float color[4]) {
        static const float NONE[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        return sameColor(color, NONE);
    }

    void addColor(std::vector<ColorCount> &counts, const float color[4], const BlockState *block, int32_t amount) {
        for (ColorCount &count: counts) {
            if (count.mBlock == block && sameColor(count.mColor, color)) {
                count.mCount += amount;
                return;
            }
        }

        ColorCount count;
        std::memcpy(count.mColor, color, sizeof(count.mColor));
        count.mBlock = block;
        count.mCount = amount;
        counts.push_back(count);
    }

    int32_t truncatingDivide(int32_t value, int32_t divisor) {
        return value / divisor;
    }
}

bool MapRenderer::isNetherStone(int32_t x, int32_t z) {
    const uint32_t mixed = (uint32_t) x + 231871u * (uint32_t) z;
    return ((mixed * (1926993u * mixed + 11u)) & 0x100000u) != 0;
}

uint32_t MapRenderer::shade(const float color[4], int32_t brightness) {
    const float factor = BRIGHTNESS[std::min(2, std::max(0, brightness))];
    const float red = std::max(0.0f, color[0] * factor);
    const float green = std::max(0.0f, color[1] * factor);
    const float blue = std::min(1.0f, std::max(0.0f, color[2] * factor));
    const uint32_t r = red > 1.0f ? 255u : (uint32_t) (int32_t) (red * 255.0f);
    const uint32_t g = green > 1.0f ? 255u : (uint32_t) (int32_t) (green * 255.0f);
    const uint32_t b = (uint32_t) (int32_t) (blue * 255.0f);
    return 0xff000000u | (b << 16) | (g << 8) | r;
}

void MapRenderer::update(MapItemSavedData &data, MapRenderSource &source, const Vector3i &playerPosition) {
    if (data.isImage())
        return;

    const int32_t size = MapItemSavedData::MAP_SIZE;
    const int32_t scale = 1 << data.getScale();
    const int32_t samples = scale * scale;
    const bool ceiling = source.hasCeiling();
    int32_t radius = size / scale;
    if (ceiling)
        radius /= 2;

    const Vector3i &center = data.getOrigin();
    const int32_t playerX = size / 2 + truncatingDivide(playerPosition.x - center.x, scale);
    const int32_t playerZ = size / 2 + truncatingDivide(playerPosition.z - center.z, scale);
    const int32_t startX = std::min(size - 1, std::max(playerX - radius + 1, 0));
    const int32_t endX = std::min(size, std::max(playerX + radius, 0));
    const int32_t startZ = std::min(size - 1, std::max(playerZ - radius - 1, -1));
    const int32_t endZ = std::min(size, std::max(playerZ + radius, 0));
    const int32_t minY = source.getMinY();
    const int32_t maxY = source.getMaxY();
    const BlockState air;
    std::vector<ColorCount> counts;
    counts.reserve(8);

    for (int32_t x = startX; x < endX; ++x) {
        float previousHeight = 0.0f;
        bool previousSampled = false;

        for (int32_t z = startZ; z < endZ; ++z) {
            if (z < -1 || z >= size)
                continue;

            const int32_t worldX = scale * (x - size / 2 + truncatingDivide(center.x, scale));
            const int32_t worldZ = scale * (truncatingDivide(center.z, scale) + z - size / 2);
            if (!source.hasChunk(worldX >> 4, worldZ >> 4))
                continue;

            counts.clear();
            float height = 0.0f;
            int32_t depth = 0;
            bool sampled = false;

            if (ceiling) {
                if (isNetherStone(worldX, worldZ))
                    addColor(counts, NETHER_STONE, nullptr, 100);
                else
                    addColor(counts, NETHER_DIRT, nullptr, 10);
                height = 100.0f;
            } else {
                for (int32_t offsetX = 0; offsetX < scale; ++offsetX) {
                    for (int32_t offsetZ = 0; offsetZ < scale; ++offsetZ) {
                        const int32_t blockX = worldX + offsetX;
                        const int32_t blockZ = worldZ + offsetZ;
                        if (!source.hasChunk(blockX >> 4, blockZ >> 4))
                            continue;

                        int32_t y = std::min(source.getSurfaceY(blockX, blockZ) + 1, maxY);
                        float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                        const BlockState *block = &air;
                        if (y > minY + 1) {
                            do {
                                --y;
                                block = &source.getBlock(blockX, y, blockZ);
                                MapColors::getColor(*block, source.getBiome(blockX, y, blockZ), color);
                            } while (isNone(color) && y > minY);

                            if (y > 0 && MapColors::isLiquid(block->mName)) {
                                int32_t below = y - 1;
                                while (true) {
                                    const BlockState &liquid = source.getBlock(blockX, below, blockZ);
                                    ++depth;
                                    if (below < 2 || !MapColors::isLiquid(liquid.mName))
                                        break;
                                    --below;
                                }
                            }
                        }

                        addColor(counts, color, block, 1);
                        height += (float) (y / samples);
                        sampled = true;
                    }
                }
            }

            const int32_t waterDepth = depth / samples;
            if (!previousSampled)
                previousHeight = height;

            const int32_t parity = (x + z) & 1;
            const float slope = ((float) parity - 0.5f) * 0.4f
                                + ((height - previousHeight) * 4.0f) / (float) (scale + 4);
            int32_t brightness = slope > 0.6f ? 2 : 1;
            if (slope < -0.6f)
                brightness = 0;

            float best[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            const BlockState *bestBlock = &air;
            int32_t bestCount = 0;
            for (const ColorCount &count: counts) {
                if (count.mCount > bestCount) {
                    std::memcpy(best, count.mColor, sizeof(best));
                    bestBlock = count.mBlock;
                    bestCount = count.mCount;
                }
            }

            if (bestBlock != nullptr && MapColors::isWater(bestBlock->mName)) {
                const float water = (float) parity * 0.2f + (float) waterDepth * 0.1f;
                brightness = water > 0.9f ? 0 : (water < 0.5f ? 2 : 1);
            }

            previousSampled = sampled;
            const uint32_t distance = (uint32_t) ((x - playerX) * (x - playerX) + (z - playerZ) * (z - playerZ));
            const bool edge = parity == 0;
            const bool drawn = z >= 0 && distance < (uint32_t) (radius * radius)
                               && (!edge || distance <= (uint32_t) ((radius - 2) * (radius - 2)));
            if (drawn)
                data.setPixel(x, z, shade(best, brightness));

            previousHeight = height;
        }
    }
}
