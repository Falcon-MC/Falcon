#include "Level/Generator/Overworld/Feature/Terrain/CaveFeature.h"

#include "Block/Blocks/VanillaBlocks.h"
#include "Core/Math/MathConstants.h"
#include "Level/Generator/Feature/FeatureMath.h"
#include "Level/Generator/Random/SimpleRandom.h"
#include "Level/Generator/Overworld/Surface/SurfaceMaterialAttributes.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"

namespace {
    const int32_t DIMENSION_MIN_HEIGHT = -64;
    const int32_t DIMENSION_MAX_HEIGHT = 319;
}

const BlockState &CaveFeature::airState() {
    static const BlockState state = VanillaBlocks::AIR().toBlockState();
    return state;
}

const BlockState &CaveFeature::lavaState() {
    static const BlockState state = VanillaBlocks::LAVA().toBlockState();
    return state;
}

bool CaveFeature::isLiquidState(const BlockState &state) {
    return state.mName == "minecraft:water" || state.mName == "minecraft:flowing_water"
           || state.mName == "minecraft:lava" || state.mName == "minecraft:flowing_lava";
}

const char *CaveFeature::name() const {
    return "minecraft:overworld_cave";
}

void CaveFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    mRandom.setSeed(level.getSeed());
    const int64_t xSeed = mRandom.nextLong();
    const int64_t zSeed = mRandom.nextLong();

    for (int32_t sourceChunkX = chunkX - mCarvingRangeChunks; sourceChunkX <= chunkX + mCarvingRangeChunks;
         sourceChunkX++) {
        for (int32_t sourceChunkZ = chunkZ - mCarvingRangeChunks; sourceChunkZ <= chunkZ + mCarvingRangeChunks;
             sourceChunkZ++) {
            const int64_t carvingSeed = (int64_t) ((uint64_t) (int64_t) sourceChunkX * (uint64_t) xSeed)
                                        ^ (int64_t) ((uint64_t) (int64_t) sourceChunkZ * (uint64_t) zSeed)
                                        ^ level.getSeed();
            mRandom.setSeed(carvingSeed);
            carveChunk(mRandom, sourceChunkX, sourceChunkZ, chunk);
        }
    }
}

void CaveFeature::carveChunk(IRandom &random, int32_t sourceChunkX, int32_t sourceChunkZ, LevelChunk &chunk) {
    if (random.nextFloat() > getCaveProbability())
        return;

    const int32_t minY = DIMENSION_MIN_HEIGHT;
    const int32_t maxY = DIMENSION_MAX_HEIGHT;
    const int32_t caveMinY = FeatureMath::clampInt(minY + LAVA_LEVEL_OFFSET, minY + 1, maxY - 1);
    const int32_t caveMaxY = FeatureMath::clampInt(getCaveMaxY(), caveMinY, maxY - 1);
    const int32_t lavaLevel = minY + LAVA_LEVEL_OFFSET;
    const int32_t maxDistance = mCarvingRangeChunks * CHUNK_SIZE - CHUNK_SIZE;
    const int32_t caveCount = random.nextInt(random.nextInt(random.nextInt(getCaveBound()) + 1) + 1);

    for (int32_t cave = 0; cave < caveCount; cave++) {
        const double x = (double) (sourceChunkX * CHUNK_SIZE + random.nextInt(CHUNK_SIZE));
        const double y = (double) random.nextInt(caveMinY, caveMaxY + 1);
        const double z = (double) (sourceChunkZ * CHUNK_SIZE + random.nextInt(CHUNK_SIZE));
        const double horizontalRadiusMultiplier = 0.7 + (double) random.nextFloat() * 0.7;
        const double verticalRadiusMultiplier = 0.8 + (double) random.nextFloat() * 0.5;
        const double floorLevel = -1.0 + (double) random.nextFloat() * 0.6;

        int32_t tunnels = 1;
        if (random.nextInt(4) == 0) {
            const double yScale = 0.1 + (double) random.nextFloat() * 0.8;
            const float thickness = 1.0f + random.nextFloat() * 6.0f;
            createRoom(chunk, x, y, z, thickness, yScale, floorLevel, minY, maxY, lavaLevel);
            tunnels += random.nextInt(4);
        }

        for (int32_t i = 0; i < tunnels; i++) {
            const float horizontalRotation = random.nextFloat() * (MathConstants::PI_F * 2.0f);
            const float verticalRotation = (random.nextFloat() - 0.5f) / 4.0f;
            const float thickness = getThickness(random);
            const int32_t distance = maxDistance - random.nextInt(maxDistance / 4);
            createTunnel(random.nextLong(), chunk, x, y, z, horizontalRadiusMultiplier, verticalRadiusMultiplier,
                         thickness, horizontalRotation, verticalRotation, 0, distance, getYScale(), floorLevel,
                         minY, maxY, lavaLevel);
        }
    }
}

float CaveFeature::getCaveProbability() const {
    return 0.15f;
}

int32_t CaveFeature::getCaveBound() const {
    return 15;
}

int32_t CaveFeature::getCaveMaxY() const {
    return 180;
}

float CaveFeature::getThickness(IRandom &random) const {
    float thickness = random.nextFloat() * 2.0f + random.nextFloat();
    if (random.nextInt(10) == 0)
        thickness *= random.nextFloat() * random.nextFloat() * 3.0f + 1.0f;

    return thickness;
}

double CaveFeature::getYScale() const {
    return 1.0;
}

void CaveFeature::createRoom(LevelChunk &chunk, double x, double y, double z, float thickness, double yScale,
                             double floorLevel, int32_t minY, int32_t maxY, int32_t lavaLevel) {
    const double horizontalRadius = 1.5 + (double) (FeatureMath::sinLookup((float) (MathConstants::PI / 2.0))
                                                    * thickness);
    const double verticalRadius = horizontalRadius * yScale;
    carveEllipsoid(chunk, x + 1.0, y, z, horizontalRadius, verticalRadius, floorLevel, minY, maxY, lavaLevel);
}

void CaveFeature::createTunnel(int64_t tunnelSeed, LevelChunk &chunk, double x, double y, double z,
                               double horizontalRadiusMultiplier, double verticalRadiusMultiplier, float thickness,
                               float horizontalRotation, float verticalRotation, int32_t step, int32_t dist,
                               double yScale, double floorLevel, int32_t minY, int32_t maxY, int32_t lavaLevel) {
    SimpleRandom random(tunnelSeed);
    const int32_t splitPoint = random.nextInt(dist / 2) + dist / 4;
    const bool steep = random.nextInt(6) == 0;
    float yRota = 0.0f;
    float xRota = 0.0f;
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();
    const double centerX = (double) (chunkX * CHUNK_SIZE + 8);
    const double centerZ = (double) (chunkZ * CHUNK_SIZE + 8);

    for (int32_t currentStep = step; currentStep < dist; currentStep++) {
        const double horizontalRadius = 1.5 + (double) (FeatureMath::sinLookup(
                MathConstants::PI_F * (float) currentStep / (float) dist) * thickness);
        const double verticalRadius = horizontalRadius * yScale;
        const float cosX = FeatureMath::cosLookup(verticalRotation);
        x += (double) (FeatureMath::cosLookup(horizontalRotation) * cosX);
        y += (double) FeatureMath::sinLookup(verticalRotation);
        z += (double) (FeatureMath::sinLookup(horizontalRotation) * cosX);
        verticalRotation *= steep ? 0.92f : 0.7f;
        verticalRotation += xRota * 0.1f;
        horizontalRotation += yRota * 0.1f;
        xRota *= 0.9f;
        yRota *= 0.75f;
        xRota += (random.nextFloat() - random.nextFloat()) * random.nextFloat() * 2.0f;
        yRota += (random.nextFloat() - random.nextFloat()) * random.nextFloat() * 4.0f;

        if (currentStep == splitPoint && thickness > 1.0f) {
            const int64_t firstSeed = random.nextLong();
            const float firstThickness = random.nextFloat() * 0.5f + 0.5f;
            createTunnel(firstSeed, chunk, x, y, z, horizontalRadiusMultiplier, verticalRadiusMultiplier,
                         firstThickness, horizontalRotation - (float) (MathConstants::PI / 2.0),
                         verticalRotation / 3.0f, currentStep, dist, 1.0, floorLevel, minY, maxY, lavaLevel);
            const int64_t secondSeed = random.nextLong();
            const float secondThickness = random.nextFloat() * 0.5f + 0.5f;
            createTunnel(secondSeed, chunk, x, y, z, horizontalRadiusMultiplier, verticalRadiusMultiplier,
                         secondThickness, horizontalRotation + (float) (MathConstants::PI / 2.0),
                         verticalRotation / 3.0f, currentStep, dist, 1.0, floorLevel, minY, maxY, lavaLevel);
            return;
        }

        if (random.nextInt(4) != 0) {
            if (!canReach(centerX, centerZ, x, z, currentStep, dist, thickness))
                return;

            carveEllipsoid(chunk, x, y, z, horizontalRadius * horizontalRadiusMultiplier,
                           verticalRadius * verticalRadiusMultiplier, floorLevel, minY, maxY, lavaLevel);
        }
    }
}

bool CaveFeature::canReach(double centerX, double centerZ, double x, double z, int32_t currentStep, int32_t distance,
                           float thickness) const {
    const double dx = x - centerX;
    const double dz = z - centerZ;
    const double remaining = (double) (distance - currentStep);
    const double maxReach = (double) (thickness + 2.0f + (float) CHUNK_SIZE);
    return dx * dx + dz * dz - remaining * remaining <= maxReach * maxReach;
}

bool CaveFeature::hasLiquid(LevelChunk &chunk, int32_t xFrom, int32_t xTo, int32_t yFrom, int32_t yTo, int32_t zFrom,
                            int32_t zTo, int32_t maxY) const {
    for (int32_t xx = xFrom; xx < xTo; xx++) {
        for (int32_t zz = zFrom; zz < zTo; zz++) {
            for (int32_t yy = yTo + 1; yy >= yFrom - 1; yy--) {
                if (yy >= maxY)
                    continue;

                if (yy >= LevelChunk::MIN_Y && isLiquidState(chunk.getBlock(xx, yy, zz)))
                    return true;

                if (yy != yFrom - 1 && xx != xFrom && xx != xTo - 1 && zz != zFrom && zz != zTo - 1)
                    yy = yFrom;
            }
        }
    }

    return false;
}

void CaveFeature::carveEllipsoid(LevelChunk &chunk, double x, double y, double z, double horizontalRadius,
                                 double verticalRadius, double floorLevel, int32_t minY, int32_t maxY,
                                 int32_t lavaLevel) {
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();
    const int32_t chunkXBlock = chunkX * CHUNK_SIZE;
    const int32_t chunkZBlock = chunkZ * CHUNK_SIZE;
    int32_t xFrom = FeatureMath::floorDouble(x - horizontalRadius) - chunkXBlock - 1;
    int32_t xTo = FeatureMath::floorDouble(x + horizontalRadius) - chunkXBlock + 1;
    int32_t yFrom = FeatureMath::floorDouble(y - verticalRadius) - 1;
    int32_t yTo = FeatureMath::floorDouble(y + verticalRadius) + 1;
    int32_t zFrom = FeatureMath::floorDouble(z - horizontalRadius) - chunkZBlock - 1;
    int32_t zTo = FeatureMath::floorDouble(z + horizontalRadius) - chunkZBlock + 1;

    if (xFrom < 0)
        xFrom = 0;

    if (xTo > CHUNK_SIZE)
        xTo = CHUNK_SIZE;

    if (zFrom < 0)
        zFrom = 0;

    if (zTo > CHUNK_SIZE)
        zTo = CHUNK_SIZE;

    if (yFrom < minY + 1)
        yFrom = minY + 1;

    if (yTo > maxY - 1)
        yTo = maxY - 1;

    if (xFrom >= xTo || zFrom >= zTo || yFrom >= yTo)
        return;

    if (hasLiquid(chunk, xFrom, xTo, yFrom, yTo, zFrom, zTo, maxY))
        return;

    const double invHorizontalRadius = 1.0 / horizontalRadius;
    const double invVerticalRadius = 1.0 / verticalRadius;

    for (int32_t xx = xFrom; xx < xTo; xx++) {
        const double xd = ((double) (xx + chunkXBlock) + 0.5 - x) * invHorizontalRadius;
        const double xdSq = xd * xd;

        for (int32_t zz = zFrom; zz < zTo; zz++) {
            const double zd = ((double) (zz + chunkZBlock) + 0.5 - z) * invHorizontalRadius;
            const double horizontalSq = xdSq + zd * zd;
            if (horizontalSq >= 1.0)
                continue;

            bool grassFound = false;
            for (int32_t yy = yTo; yy > yFrom; yy--) {
                const double yd = ((double) yy - 0.5 - y) * invVerticalRadius;
                if (yd <= floorLevel || horizontalSq + yd * yd >= 1.0)
                    continue;

                const BlockState currentState = chunk.getBlock(xx, yy, zz);
                if (currentState.mName == "minecraft:grass_block")
                    grassFound = true;

                if (yy <= lavaLevel) {
                    if (!(currentState == lavaState()))
                        chunk.setBlock(xx, yy, zz, lavaState());
                } else {
                    if (!(currentState == airState()))
                        chunk.setBlock(xx, yy, zz, airState());

                    restoreSurfaceIfNeeded(chunk, xx, yy - 1, zz, grassFound);
                }
            }
        }
    }
}

void CaveFeature::restoreSurfaceIfNeeded(LevelChunk &chunk, int32_t x, int32_t y, int32_t z, bool grassFound) {
    if (!grassFound)
        return;

    if (y + 1 > LevelChunk::MAX_Y || y < LevelChunk::MIN_Y)
        return;

    if (!(chunk.getBlock(x, y + 1, z) == airState()))
        return;

    const BlockState belowState = chunk.getBlock(x, y, z);
    if (!isSupportDirt(belowState) || belowState.mName == "minecraft:grass_block")
        return;

    const SurfaceBuilderComponent *component = SurfaceBuilderComponent::getForBiome(
            (int32_t) chunk.getBiomeAt(x, y, z));
    if (component == nullptr || component->mSurfaceMaterial.mTopBlock == nullptr)
        return;

    chunk.setBlock(x, y, z, *component->mSurfaceMaterial.mTopBlock);
}
