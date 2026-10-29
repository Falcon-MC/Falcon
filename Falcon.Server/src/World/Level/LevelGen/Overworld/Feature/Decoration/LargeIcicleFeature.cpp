#include "World/Level/LevelGen/Overworld/Feature/Decoration/LargeIcicleFeature.h"

#include "Core/Math/MathConstants.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Feature/FeatureMath.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/IceCavesSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <algorithm>
#include <climits>
#include <cmath>

// Port of the 26.60 large speleothem feature (the class behind large_dripstone_feature) with the
// parameters registered for minecraft:large_icicle_feature. The cones are made of packed ice.
namespace {
    const int32_t MIN_ATTEMPTS = 48;
    const int32_t EXTRA_ATTEMPTS = 16;
    const int32_t MIN_Y = -64;
    const int32_t HEIGHT_RANGE = 320;

    const int32_t SEARCH_RANGE = 30;
    const int32_t MIN_CAVE_HEIGHT = 4;
    const float MAX_RADIUS_TO_CAVE_HEIGHT = 0.33f;
    const int32_t MIN_RADIUS = 3;
    const int32_t MAX_RADIUS = 18;
    const float MIN_STALACTITE_BLUNTNESS = 0.3f;
    const float MAX_STALACTITE_BLUNTNESS = 0.9f;
    const float MIN_STALAGMITE_BLUNTNESS = 0.4f;
    const float MAX_STALAGMITE_BLUNTNESS = 1.0f;
    const float MIN_HEIGHT_SCALE = 0.4f;
    const float MAX_HEIGHT_SCALE = 2.0f;
    const float MAX_WIND_SPEED = 0.3f;
    const int32_t MIN_RADIUS_FOR_WIND = 4;
    const float MIN_BLUNTNESS_FOR_WIND = 0.6f;
    const int32_t MAX_ROOT_SEARCH = 10;

    struct Wind {
        bool mActive = false;
        int32_t mOriginY = 0;
        float mX = 0.0f;
        float mZ = 0.0f;

        int32_t offsetX(int32_t x, int32_t y) const {
            return mActive ? x + (int32_t) ((float) (mOriginY - y) * mX) : x;
        }

        int32_t offsetZ(int32_t z, int32_t y) const {
            return mActive ? z + (int32_t) ((float) (mOriginY - y) * mZ) : z;
        }
    };

    struct Speleothem {
        int32_t mX;
        int32_t mY;
        int32_t mZ;
        bool mStalagmite;
        int32_t mRadius;
        float mBluntness;
        float mScale;
    };

    bool isAirOrWater(const BlockState &state) {
        return DecorationSupport::isAir(state) || DecorationSupport::isWater(state);
    }

    bool isOpen(const BlockState &state) {
        return isAirOrWater(state) || IceCavesSupport::isLava(state);
    }

    bool isEndpoint(const BlockState &state) {
        return state.mName == "minecraft:calcite" || state.mName == "minecraft:stone"
               || state.mName == "minecraft:deepslate" || state == IceCavesSupport::packedIceState()
               || IceCavesSupport::isLava(state);
    }

    bool isSpeleothem(const BlockState &state) {
        return state.mName == "minecraft:pointed_dripstone" || state.mName == "minecraft:sulfur_spike"
               || state.mName == "minecraft:icicle";
    }

    bool scanColumn(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t step, int32_t &endY) {
        int32_t steps = 0;
        int32_t currentY = y + step;
        while (steps < SEARCH_RANGE - 1 && currentY >= LevelChunk::MIN_Y && currentY <= LevelChunk::MAX_Y
               && isAirOrWater(manager.getBlockAt(x, currentY, z))) {
            currentY += step;
            steps++;
        }

        if (!isEndpoint(manager.getBlockAt(x, currentY, z)))
            return false;

        endY = y + step * (steps + 1);
        return true;
    }

    float coneHeight(float distance, int32_t radius, float scale, float bluntness) {
        const double ratio = (double) (std::max(bluntness, distance) / (float) radius) * 0.384;
        const double shape = std::pow(ratio, 4.0 / 3.0) * 0.75 - std::pow(ratio, 2.0 / 3.0)
                             - std::log(ratio) / 3.0;
        return (float) radius * (float) (std::max(shape * scale, 0.0) / 0.384);
    }

    bool isCircleMostlyEmbedded(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t radius) {
        if (isOpen(manager.getBlockAt(x, y, z)))
            return false;

        const float step = 6.0f / (float) radius;
        for (float angle = 0.0f; angle < MathConstants::TWO_PI_F; angle += step) {
            const int32_t offsetX = (int32_t) (FeatureMath::cosLookup(angle) * (float) radius);
            const int32_t offsetZ = (int32_t) (FeatureMath::sinLookup(angle) * (float) radius);
            if (isOpen(manager.getBlockAt(x + offsetX, y, z + offsetZ)))
                return false;
        }
        return true;
    }

    // Moves the root into the rock it hangs from, halving the radius until a ring of that size is
    // mostly buried.
    bool settleRoot(BlockManager &manager, Speleothem &speleothem, const Wind &wind) {
        while (speleothem.mRadius >= 2) {
            const int32_t height = (int32_t) coneHeight(0.0f, speleothem.mRadius, speleothem.mScale,
                                                        speleothem.mBluntness);
            const int32_t attempts = std::min(height, MAX_ROOT_SEARCH);
            for (int32_t i = 0; i < attempts; i++) {
                const int32_t y = speleothem.mY + (speleothem.mStalagmite ? -i : i);
                if (IceCavesSupport::isLava(manager.getBlockAt(speleothem.mX, y, speleothem.mZ)))
                    return false;

                if (isCircleMostlyEmbedded(manager, wind.offsetX(speleothem.mX, y), y,
                                           wind.offsetZ(speleothem.mZ, y), speleothem.mRadius)) {
                    speleothem.mY = y;
                    return true;
                }
            }

            const int32_t previous = speleothem.mRadius;
            speleothem.mRadius /= 2;
            if (previous <= 3)
                return false;
        }
        return false;
    }

    void placeCone(BlockManager &manager, IRandom &random, const Speleothem &speleothem, const Wind &wind) {
        const int32_t radius = speleothem.mRadius;
        const int32_t step = speleothem.mStalagmite ? 1 : -1;

        for (int32_t dx = -radius; dx <= radius; dx++) {
            for (int32_t dz = -radius; dz <= radius; dz++) {
                const float distance = std::sqrt((float) (dx * dx + dz * dz));
                if (distance > (float) radius)
                    continue;

                int32_t height = (int32_t) coneHeight(distance, radius, speleothem.mScale, speleothem.mBluntness);
                if (height <= 0)
                    continue;

                if (random.nextFloat() < 0.2f)
                    height = (int32_t) ((random.nextFloat() * 0.19999999f + 0.8f) * (float) height);

                const int32_t x = speleothem.mX + dx;
                const int32_t z = speleothem.mZ + dz;
                const int32_t limit = speleothem.mStalagmite ? manager.getHeightAt(x, z) : INT_MAX;

                bool placed = false;
                int32_t y = speleothem.mY;
                for (int32_t i = 0; i < height && y < limit; i++, y += step) {
                    const int32_t placeX = wind.offsetX(x, y);
                    const int32_t placeZ = wind.offsetZ(z, y);
                    const BlockState &current = manager.getBlockAt(placeX, y, placeZ);
                    if (isOpen(current) || isSpeleothem(current)) {
                        manager.setBlockStateAt(placeX, y, placeZ, IceCavesSupport::packedIceState());
                        placed = true;
                    } else if (placed) {
                        break;
                    }
                }
            }
        }
    }

    void placeLargeIcicle(BlockManager &manager, IRandom &random, int32_t originX, int32_t originY, int32_t originZ) {
        if (!isAirOrWater(manager.getBlockAt(originX, originY, originZ)))
            return;

        int32_t ceilingY = 0;
        int32_t floorY = 0;
        const bool hasCeiling = scanColumn(manager, originX, originY, originZ, 1, ceilingY);
        const bool hasFloor = scanColumn(manager, originX, originY, originZ, -1, floorY);
        if (!hasCeiling || !hasFloor || ceilingY - floorY <= MIN_CAVE_HEIGHT)
            return;

        const int32_t limit = (int32_t) ((float) (ceilingY - floorY - 1) * MAX_RADIUS_TO_CAVE_HEIGHT);
        const int32_t maxRadius = std::min(std::max(limit, MIN_RADIUS), MAX_RADIUS);
        const int32_t radius = MIN_RADIUS + random.nextInt(maxRadius - MIN_RADIUS + 1);

        Speleothem stalactite{originX, ceilingY - 1, originZ, false, radius, 0.0f, 0.0f};
        stalactite.mBluntness = MIN_STALACTITE_BLUNTNESS
                                + (MAX_STALACTITE_BLUNTNESS - MIN_STALACTITE_BLUNTNESS) * random.nextFloat();
        stalactite.mScale = MIN_HEIGHT_SCALE + (MAX_HEIGHT_SCALE - MIN_HEIGHT_SCALE) * random.nextFloat();

        Speleothem stalagmite{originX, floorY + 1, originZ, true, radius, 0.0f, 0.0f};
        stalagmite.mBluntness = MIN_STALAGMITE_BLUNTNESS
                                + (MAX_STALAGMITE_BLUNTNESS - MIN_STALAGMITE_BLUNTNESS) * random.nextFloat();
        stalagmite.mScale = MIN_HEIGHT_SCALE + (MAX_HEIGHT_SCALE - MIN_HEIGHT_SCALE) * random.nextFloat();

        Wind wind;
        if (radius >= MIN_RADIUS_FOR_WIND && stalactite.mBluntness >= MIN_BLUNTNESS_FOR_WIND
            && stalagmite.mBluntness >= MIN_BLUNTNESS_FOR_WIND) {
            const float speed = MAX_WIND_SPEED * random.nextFloat();
            const float angle = random.nextFloat() * MathConstants::PI_F;
            wind.mActive = true;
            wind.mOriginY = originY;
            wind.mX = FeatureMath::cosLookup(angle) * speed;
            wind.mZ = FeatureMath::sinLookup(angle) * speed;
        }

        const bool stalactiteSettled = settleRoot(manager, stalactite, wind);
        const bool stalagmiteSettled = settleRoot(manager, stalagmite, wind);

        if (stalactiteSettled) {
            const int32_t tipY = stalactite.mY - (int32_t) coneHeight(0.0f, stalactite.mRadius, stalactite.mScale,
                                                                      stalactite.mBluntness);
            if (tipY > LevelChunk::MIN_Y)
                placeCone(manager, random, stalactite, wind);
        }

        if (stalagmiteSettled) {
            const int32_t tipY = stalagmite.mY + (int32_t) coneHeight(0.0f, stalagmite.mRadius, stalagmite.mScale,
                                                                      stalagmite.mBluntness);
            if (tipY < LevelChunk::MAX_Y)
                placeCone(manager, random, stalagmite, wind);
        }
    }
}

const char *LargeIcicleFeature::name() const {
    return "minecraft:large_icicle_feature";
}

void LargeIcicleFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    mRandom.setSeed(level.getSeed() ^ DecorationSupport::chunkHash(chunk.getX(), chunk.getZ())
                    ^ javaStringHash(name()));

    BlockManager manager(level);
    manager.readPendingFrom(mRoot);
    const int32_t attempts = MIN_ATTEMPTS + mRandom.nextBoundedInt(EXTRA_ATTEMPTS);
    for (int32_t i = 0; i < attempts; i++) {
        const int32_t x = (chunk.getX() << 4) + mRandom.nextBoundedInt(15);
        const int32_t y = MIN_Y + mRandom.nextBoundedInt(HEIGHT_RANGE);
        const int32_t z = (chunk.getZ() << 4) + mRandom.nextBoundedInt(15);
        if (IceCavesSupport::isIceCaves(chunk, x, y, z))
            placeLargeIcicle(manager, mRandom, x, y, z);
    }

    queueObject(manager);
}
