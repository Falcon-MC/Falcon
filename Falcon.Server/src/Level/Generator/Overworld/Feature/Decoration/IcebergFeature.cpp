#include "Level/Generator/Overworld/Feature/Decoration/IcebergFeature.h"

#include "Block/Blocks/VanillaBlocks.h"
#include "Core/Math/MathConstants.h"
#include "Level/Generator/Biome/BiomeIds.h"
#include "Level/Generator/Feature/BlockManager.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {

    const int32_t SEA_LEVEL = 63;

    const BlockState &mainState() {
        static const BlockState state = VanillaBlocks::PACKED_ICE().toBlockState();
        return state;
    }

    const BlockState &snowState() {
        static const BlockState state = VanillaBlocks::SNOW().toBlockState();
        return state;
    }

    const BlockState &waterState() {
        static const BlockState state = VanillaBlocks::WATER().toBlockState();
        return state;
    }

    const BlockState &blueIceState() {
        static const BlockState state = VanillaBlocks::BLUE_ICE().toBlockState();
        return state;
    }

    std::string idOf(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
        return manager.getBlockAt(x, y, z).mName;
    }

    void setBlockAt(BlockManager &manager, int32_t x, int32_t y, int32_t z, const BlockState &state) {
        if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y + 1)
            return;

        manager.setBlockStateAt(x, y, z, state);
    }

    float clampFloat(float value, float min, float max) {
        if (value < min)
            return min;

        if (value > max)
            return max;

        return value;
    }

    int64_t posKey(int32_t x, int32_t y, int32_t z) {
        return (((int64_t) x) << 42) ^ ((((int64_t) y) & 0x1FFFFF) << 21) ^ (((int64_t) z) & 0x1FFFFF);
    }

    bool isWaterId(const std::string &id) {
        return id == "minecraft:water" || id == "minecraft:flowing_water";
    }

    bool isIcebergId(const std::string &id) {
        return id == "minecraft:packed_ice" || id == "minecraft:snow" || id == "minecraft:blue_ice";
    }

    double signedDistanceEllipse(int32_t xo, int32_t zo, int32_t originX, int32_t originZ, int32_t a, int32_t c,
                                 double angle) {
        const double first = ((double) (xo - originX) * std::cos(angle) - (double) (zo - originZ) * std::sin(angle))
                             / (double) std::max(1, a);
        const double second = ((double) (xo - originX) * std::sin(angle) + (double) (zo - originZ) * std::cos(angle))
                              / (double) std::max(1, c);
        return first * first + second * second - 1.0;
    }

    int32_t ellipseCFor(int32_t yOff, int32_t height, int32_t shapeEllipseC) {
        if (yOff > 0 && height - yOff <= 3)
            return shapeEllipseC - (4 - (height - yOff));

        return shapeEllipseC;
    }

}

IcebergFeature::IcebergFeature() : mLegacyRandom(0) {
}

const char *IcebergFeature::name() const {
    return "minecraft:frozen_ocean_after_surface_ice_feature";
}

int32_t IcebergFeature::_nextIntExclusive(int32_t boundExclusive) {
    if (boundExclusive <= 1)
        return 0;

    return mLegacyRandom.nextRange(0, boundExclusive - 1);
}

double IcebergFeature::_signedDistanceCircle(int32_t xo, int32_t zo, int32_t originX, int32_t originZ,
                                             int32_t radius) {
    const float off = 10.0f * clampFloat(mLegacyRandom.nextFloat(), 0.2f, 0.8f) / (float) std::max(1, radius);
    return (double) off + std::pow((double) (xo - originX), 2.0) + std::pow((double) (zo - originZ), 2.0)
           - std::pow((double) radius, 2.0);
}

int32_t IcebergFeature::_heightDependentRadiusRound(int32_t yOff, int32_t height, int32_t width) {
    const float k = 3.5f - mLegacyRandom.nextFloat();
    float scale = (1.0f - (float) std::pow((double) yOff, 2.0) / ((float) height * k)) * (float) width;

    if (height > 15 + _nextIntExclusive(5)) {
        const int32_t tempYOff = yOff < 3 + _nextIntExclusive(6) ? yOff / 2 : yOff;
        scale = (1.0f - (float) tempYOff / ((float) height * k * 0.4f)) * (float) width;
    }

    return (int32_t) std::ceil((double) (scale / 2.0f));
}

int32_t IcebergFeature::_heightDependentRadiusEllipse(int32_t yOff, int32_t height, int32_t width) {
    const float scale = (1.0f - (float) std::pow((double) yOff, 2.0) / ((float) height * 1.0f)) * (float) width;
    return (int32_t) std::ceil((double) (scale / 2.0f));
}

int32_t IcebergFeature::_heightDependentRadiusSteep(int32_t yOff, int32_t height, int32_t width) {
    const float k = 1.0f + mLegacyRandom.nextFloat() / 2.0f;
    const float scale = (1.0f - (float) yOff / ((float) height * k)) * (float) width;
    return (int32_t) std::ceil((double) (scale / 2.0f));
}

void IcebergFeature::_setIcebergBlock(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t hDiff,
                                      int32_t height, bool isEllipse, bool snowOnTop,
                                      std::set<int64_t> &localIceberg) {
    const std::string state = idOf(manager, x, y, z);
    if (state != "minecraft:air" && state != "minecraft:snow" && state != "minecraft:ice"
        && state != "minecraft:water" && state != "minecraft:flowing_water")
        return;

    const bool randomness = !isEllipse || mLegacyRandom.nextDouble() > 0.05;
    const int32_t divisor = isEllipse ? 3 : 2;

    if (snowOnTop && state != "minecraft:water" && state != "minecraft:flowing_water"
        && hDiff <= _nextIntExclusive(std::max(1, height / divisor)) + (int32_t) ((float) height * 0.6f)
        && randomness) {
        setBlockAt(manager, x, y, z, snowState());
        localIceberg.insert(posKey(x, y, z));
        return;
    }

    setBlockAt(manager, x, y, z, mainState());
    localIceberg.insert(posKey(x, y, z));
}

void IcebergFeature::_generateIcebergBlock(BlockManager &manager, int32_t originX, int32_t originY, int32_t originZ,
                                           int32_t height, int32_t xo, int32_t yOff, int32_t zo, int32_t radius,
                                           int32_t a, bool isEllipse, int32_t shapeEllipseC, double shapeAngle,
                                           bool snowOnTop, std::set<int64_t> &localIceberg) {
    const double signedDist = isEllipse
                              ? signedDistanceEllipse(xo, zo, 0, 0, a, ellipseCFor(yOff, height, shapeEllipseC),
                                                      shapeAngle)
                              : _signedDistanceCircle(xo, zo, 0, 0, radius);
    if (signedDist >= 0.0)
        return;

    const int32_t x = originX + xo;
    const int32_t y = originY + yOff;
    const int32_t z = originZ + zo;
    const double compareVal = isEllipse ? -0.5 : (double) (-6 - _nextIntExclusive(3));
    if (signedDist > compareVal && mLegacyRandom.nextDouble() > 0.9)
        return;

    _setIcebergBlock(manager, x, y, z, height - yOff, height, isEllipse, snowOnTop, localIceberg);
}

void IcebergFeature::_removeFloatingSnowLayer(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    if (idOf(manager, x, y + 1, z) == "minecraft:snow_layer")
        setBlockAt(manager, x, y + 1, z, DecorationSupport::airState());
}

void IcebergFeature::_carve(BlockManager &manager, int32_t radius, int32_t yOff, int32_t originX, int32_t originY,
                            int32_t originZ, bool underWater, double angle, int32_t localOriginX,
                            int32_t localOriginZ, int32_t shapeEllipseA, int32_t shapeEllipseC) {
    const int32_t a = radius + 1 + shapeEllipseA / 3;
    const int32_t c = std::min(radius - 3, 3) + shapeEllipseC / 2 - 1;
    if (c <= 0)
        return;

    for (int32_t xo = -a; xo < a; xo++) {
        for (int32_t zo = -a; zo < a; zo++) {
            const double signedDist = signedDistanceEllipse(xo, zo, localOriginX, localOriginZ, a, c, angle);
            if (signedDist >= 0.0)
                continue;

            const int32_t x = originX + xo;
            const int32_t y = originY + yOff;
            const int32_t z = originZ + zo;
            const std::string state = idOf(manager, x, y, z);
            if (!isIcebergId(state) && state != "minecraft:snow")
                continue;

            if (underWater) {
                setBlockAt(manager, x, y, z, waterState());
            } else {
                setBlockAt(manager, x, y, z, DecorationSupport::airState());
                _removeFloatingSnowLayer(manager, x, y, z);
            }
        }
    }
}

void IcebergFeature::_generateCutOut(BlockManager &manager, int32_t width, int32_t height, int32_t originX,
                                     int32_t originY, int32_t originZ, bool isEllipse, int32_t shapeEllipseA,
                                     double shapeAngle, int32_t shapeEllipseC) {
    const int32_t randomSignX = mLegacyRandom.nextBoolean() ? -1 : 1;
    const int32_t randomSignZ = mLegacyRandom.nextBoolean() ? -1 : 1;

    int32_t xOff = _nextIntExclusive(std::max(width / 2 - 2, 1));
    if (mLegacyRandom.nextBoolean())
        xOff = width / 2 + 1 - _nextIntExclusive(std::max(width - width / 2 - 1, 1));

    int32_t zOff = _nextIntExclusive(std::max(width / 2 - 2, 1));
    if (mLegacyRandom.nextBoolean())
        zOff = width / 2 + 1 - _nextIntExclusive(std::max(width - width / 2 - 1, 1));

    if (isEllipse) {
        xOff = _nextIntExclusive(std::max(shapeEllipseA - 5, 1));
        zOff = xOff;
    }

    const int32_t localOriginX = randomSignX * xOff;
    const int32_t localOriginZ = randomSignZ * zOff;
    const double angle = isEllipse ? shapeAngle + (MathConstants::PI / 2.0)
                                   : mLegacyRandom.nextDouble() * 2.0 * MathConstants::PI;

    for (int32_t yOff = 0; yOff < height - 3; yOff++) {
        const int32_t radius = _heightDependentRadiusRound(yOff, height, width);
        _carve(manager, radius, yOff, originX, originY, originZ, false, angle, localOriginX, localOriginZ,
               shapeEllipseA, shapeEllipseC);
    }

    for (int32_t yOff = -1; yOff > -height + _nextIntExclusive(5); yOff--) {
        const int32_t radius = _heightDependentRadiusSteep(-yOff, height, width);
        _carve(manager, radius, yOff, originX, originY, originZ, true, angle, localOriginX, localOriginZ,
               shapeEllipseA, shapeEllipseC);
    }
}

void IcebergFeature::_generateBlueIce(BlockManager &manager, int32_t originX, int32_t originZ) {
    const int32_t x = originX + _nextIntExclusive(8) - _nextIntExclusive(8);
    const int32_t z = originZ + _nextIntExclusive(8) - _nextIntExclusive(8);
    const int32_t y = SEA_LEVEL - 2;

    const std::string originState = idOf(manager, x, y, z);
    const std::string belowState = idOf(manager, x, y - 1, z);
    if (!isWaterId(originState) && !isWaterId(belowState))
        return;

    const bool foundPackedIce = idOf(manager, x + 1, y, z) == "minecraft:packed_ice"
                                || idOf(manager, x - 1, y, z) == "minecraft:packed_ice"
                                || idOf(manager, x, y, z + 1) == "minecraft:packed_ice"
                                || idOf(manager, x, y, z - 1) == "minecraft:packed_ice"
                                || idOf(manager, x, y + 1, z) == "minecraft:packed_ice";
    if (!foundPackedIce)
        return;

    setBlockAt(manager, x, y, z, blueIceState());

    for (int32_t i = 0; i < 200; i++) {
        const int32_t yOff = _nextIntExclusive(5) - _nextIntExclusive(6);
        int32_t xzDiff = 3;
        if (yOff < 2)
            xzDiff += yOff / 2;

        if (xzDiff < 1)
            continue;

        const int32_t px = x + _nextIntExclusive(xzDiff) - _nextIntExclusive(xzDiff);
        const int32_t py = y + yOff;
        const int32_t pz = z + _nextIntExclusive(xzDiff) - _nextIntExclusive(xzDiff);
        const std::string placeState = idOf(manager, px, py, pz);

        if (placeState != "minecraft:air" && !isWaterId(placeState) && placeState != "minecraft:packed_ice"
            && placeState != "minecraft:ice")
            continue;

        const bool adjacent = idOf(manager, px + 1, py, pz) == "minecraft:blue_ice"
                              || idOf(manager, px - 1, py, pz) == "minecraft:blue_ice"
                              || idOf(manager, px, py, pz + 1) == "minecraft:blue_ice"
                              || idOf(manager, px, py, pz - 1) == "minecraft:blue_ice"
                              || idOf(manager, px, py + 1, pz) == "minecraft:blue_ice"
                              || idOf(manager, px, py - 1, pz) == "minecraft:blue_ice";
        if (adjacent)
            setBlockAt(manager, px, py, pz, blueIceState());
    }
}

void IcebergFeature::_smooth(std::set<int64_t> &mask, BlockManager &manager, int32_t originX, int32_t originY,
                             int32_t originZ, int32_t width, int32_t height, bool isEllipse,
                             int32_t shapeEllipseA) {
    const int32_t a = isEllipse ? shapeEllipseA : width / 2;
    const int32_t smoothingPasses = 3;

    for (int32_t pass = 0; pass < smoothingPasses; pass++) {
        std::set<int64_t> toRemove;

        for (int32_t xOff = -a; xOff <= a; xOff++) {
            for (int32_t zOff = -a; zOff <= a; zOff++) {
                for (int32_t yOff = 0; yOff <= height; yOff++) {
                    const int32_t x = originX + xOff;
                    const int32_t y = originY + yOff;
                    const int32_t z = originZ + zOff;
                    const int64_t key = posKey(x, y, z);
                    if (mask.count(key) == 0)
                        continue;

                    if (mask.count(posKey(x, y - 1, z)) == 0) {
                        toRemove.insert(key);
                        const int64_t above = posKey(x, y + 1, z);
                        if (mask.count(above) != 0)
                            toRemove.insert(above);

                        continue;
                    }

                    int32_t counter = 0;
                    if (mask.count(posKey(x - 1, y, z)) == 0)
                        counter++;
                    if (mask.count(posKey(x + 1, y, z)) == 0)
                        counter++;
                    if (mask.count(posKey(x, y, z - 1)) == 0)
                        counter++;
                    if (mask.count(posKey(x, y, z + 1)) == 0)
                        counter++;

                    if (counter >= 3)
                        toRemove.insert(key);
                }
            }
        }

        if (toRemove.empty())
            break;

        for (int64_t key: toRemove) {
            int32_t x = (int32_t) (key >> 42);
            int32_t y = (int32_t) ((key >> 21) & 0x1FFFFF);
            int32_t z = (int32_t) (key & 0x1FFFFF);
            if (y >= 0x100000)
                y -= 0x200000;
            if (z >= 0x100000)
                z -= 0x200000;

            setBlockAt(manager, x, y, z, DecorationSupport::airState());
            mask.erase(key);
        }
    }
}

void IcebergFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    mLegacyRandom.setSeed(level.getSeed() ^ DecorationSupport::chunkHash(chunkX, chunkZ) ^ javaStringHash(name()));

    const int32_t originX = (chunkX << 4) + _nextIntExclusive(16);
    const int32_t originZ = (chunkZ << 4) + _nextIntExclusive(16);
    const int32_t biomeId = DecorationSupport::getBiomeAt(level, originX, SEA_LEVEL, originZ);
    if (biomeId != BiomeIds::FROZEN_OCEAN && biomeId != BiomeIds::DEEP_FROZEN_OCEAN
        && biomeId != BiomeIds::LEGACY_FROZEN_OCEAN)
        return;

    if (_nextIntExclusive(8) != 0)
        return;

    const bool snowOnTop = mLegacyRandom.nextDouble() > 0.7;
    const double shapeAngle = mLegacyRandom.nextDouble() * 2.0 * MathConstants::PI;
    const int32_t shapeEllipseA = 11 - _nextIntExclusive(5);
    const int32_t shapeEllipseC = 3 + _nextIntExclusive(3);
    const bool isEllipse = mLegacyRandom.nextDouble() > 0.7;

    const int32_t maxWidthRoundIceberg = 11;
    int32_t overWaterHeight = isEllipse ? _nextIntExclusive(6) + 6 : _nextIntExclusive(15) + 3;
    if (!isEllipse && mLegacyRandom.nextDouble() > 0.9)
        overWaterHeight += _nextIntExclusive(19) + 7;

    const int32_t underWaterHeight = std::min(overWaterHeight + _nextIntExclusive(11), 18);
    const int32_t width = std::min(overWaterHeight + _nextIntExclusive(7) - _nextIntExclusive(5),
                                   maxWidthRoundIceberg);
    const int32_t a = isEllipse ? shapeEllipseA : maxWidthRoundIceberg;

    BlockManager manager(level);
    std::set<int64_t> localIceberg;

    for (int32_t xo = -a; xo < a; xo++) {
        for (int32_t zo = -a; zo < a; zo++) {
            for (int32_t yOff = 0; yOff < overWaterHeight; yOff++) {
                const int32_t radius = isEllipse
                                       ? _heightDependentRadiusEllipse(yOff, overWaterHeight, width)
                                       : _heightDependentRadiusRound(yOff, overWaterHeight, width);
                if (isEllipse || xo < radius)
                    _generateIcebergBlock(manager, originX, SEA_LEVEL, originZ, overWaterHeight, xo, yOff, zo,
                                          radius, a, isEllipse, shapeEllipseC, shapeAngle, snowOnTop, localIceberg);
            }
        }
    }

    for (int32_t xo = -a; xo < a; xo++) {
        for (int32_t zo = -a; zo < a; zo++) {
            for (int32_t yOff = -1; yOff > -underWaterHeight; yOff--) {
                const int32_t newA = isEllipse
                                     ? (int32_t) std::ceil((double) ((float) a * (1.0f
                                       - (float) std::pow((double) yOff, 2.0) / ((float) underWaterHeight * 8.0f))))
                                     : a;
                const int32_t radius = _heightDependentRadiusSteep(-yOff, underWaterHeight, width);
                if (xo < radius)
                    _generateIcebergBlock(manager, originX, SEA_LEVEL, originZ, underWaterHeight, xo, yOff, zo,
                                          radius, newA, isEllipse, shapeEllipseC, shapeAngle, snowOnTop,
                                          localIceberg);
            }
        }
    }

    _smooth(localIceberg, manager, originX, SEA_LEVEL, originZ, width, overWaterHeight, isEllipse, shapeEllipseA);

    const bool doCutOut = isEllipse ? mLegacyRandom.nextDouble() > 0.1 : mLegacyRandom.nextDouble() > 0.7;
    if (doCutOut)
        _generateCutOut(manager, width, overWaterHeight, originX, SEA_LEVEL, originZ, isEllipse, shapeEllipseA,
                        shapeAngle, shapeEllipseC);

    _generateBlueIce(manager, originX, originZ);
    queueObject(manager);
}
