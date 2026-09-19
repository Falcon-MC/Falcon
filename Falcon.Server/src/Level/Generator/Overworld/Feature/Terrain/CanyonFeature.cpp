#include "Level/Generator/Overworld/Feature/Terrain/CanyonFeature.h"

#include "Core/Math/MathConstants.h"
#include "Level/Generator/Feature/FeatureMath.h"
#include "Level/LevelChunk.h"

#include <cmath>

namespace {
    const float CANYON_PROBABILITY = 0.01f;
    const int32_t MIN_CANYON_Y = 10;
    const int32_t MAX_CANYON_Y = 67;
    const float Y_SCALE = 3.0f;
    const int32_t DIMENSION_MIN_HEIGHT = -64;
    const int32_t DIMENSION_MAX_HEIGHT = 319;
}

const char *CanyonFeature::name() const {
    return "minecraft:canyon_carver";
}

void CanyonFeature::carveChunk(IRandom &random, int32_t sourceChunkX, int32_t sourceChunkZ, LevelChunk &chunk) {
    if (random.nextFloat() > CANYON_PROBABILITY)
        return;

    const int32_t minY = DIMENSION_MIN_HEIGHT;
    const int32_t maxY = DIMENSION_MAX_HEIGHT;
    const int32_t lavaLevel = minY + LAVA_LEVEL_OFFSET;

    const double x = (double) (sourceChunkX * CHUNK_SIZE + random.nextInt(CHUNK_SIZE));
    const double y = (double) FeatureMath::clampInt(MIN_CANYON_Y + random.nextInt(MAX_CANYON_Y - MIN_CANYON_Y + 1),
                                                    minY + 1, maxY - 1);
    const double z = (double) (sourceChunkZ * CHUNK_SIZE + random.nextInt(CHUNK_SIZE));

    const float horizontalRotation = random.nextFloat() * (MathConstants::PI_F * 2.0f);
    const float verticalRotation = random.nextFloat() * 0.25f - 0.125f;
    const float thickness = 2.0f + random.nextFloat() * 4.0f;
    const int32_t maxDistance = mCarvingRangeChunks * CHUNK_SIZE - CHUNK_SIZE;
    const int32_t distance = (int32_t) ((float) maxDistance * (0.75f + random.nextFloat() * 0.25f));

    _carveCanyon(random, chunk, x, y, z, thickness, horizontalRotation, verticalRotation, distance, minY, maxY,
                 lavaLevel);
}

void CanyonFeature::_carveCanyon(IRandom &random, LevelChunk &chunk, double x, double y, double z, float thickness,
                                 float horizontalRotation, float verticalRotation, int32_t distance, int32_t minY,
                                 int32_t maxY, int32_t lavaLevel) {
    const std::vector<float> widthFactorPerHeight = _initWidthFactors(random, minY, maxY);
    float xRota = 0.0f;
    float yRota = 0.0f;
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();
    const double centerX = (double) (chunkX * CHUNK_SIZE + 8);
    const double centerZ = (double) (chunkZ * CHUNK_SIZE + 8);

    for (int32_t currentStep = 0; currentStep < distance; currentStep++) {
        double horizontalRadius = 1.5 + (double) (FeatureMath::sinLookup(
                (float) currentStep * MathConstants::PI_F / (float) distance) * thickness);
        const double verticalRadius = _updateVerticalRadius(random, horizontalRadius * (double) Y_SCALE, distance,
                                                            currentStep);
        horizontalRadius *= (double) (0.75f + random.nextFloat() * 0.25f);

        const float xzCos = FeatureMath::cosLookup(verticalRotation);
        const float ySin = FeatureMath::sinLookup(verticalRotation);
        x += (double) (FeatureMath::cosLookup(horizontalRotation) * xzCos);
        y += (double) ySin;
        z += (double) (FeatureMath::sinLookup(horizontalRotation) * xzCos);

        verticalRotation *= 0.7f;
        verticalRotation += xRota * 0.05f;
        horizontalRotation += yRota * 0.05f;
        xRota *= 0.8f;
        yRota *= 0.5f;
        xRota += (random.nextFloat() - random.nextFloat()) * random.nextFloat() * 2.0f;
        yRota += (random.nextFloat() - random.nextFloat()) * random.nextFloat() * 4.0f;

        if (random.nextInt(4) == 0)
            continue;

        if (!canReach(centerX, centerZ, x, z, currentStep, distance, thickness))
            return;

        int32_t xFrom = FeatureMath::floorDouble(x - horizontalRadius) - chunkX * CHUNK_SIZE - 1;
        int32_t xTo = FeatureMath::floorDouble(x + horizontalRadius) - chunkX * CHUNK_SIZE + 1;
        int32_t yFrom = FeatureMath::floorDouble(y - verticalRadius) - 1;
        int32_t yTo = FeatureMath::floorDouble(y + verticalRadius) + 1;
        int32_t zFrom = FeatureMath::floorDouble(z - horizontalRadius) - chunkZ * CHUNK_SIZE - 1;
        int32_t zTo = FeatureMath::floorDouble(z + horizontalRadius) - chunkZ * CHUNK_SIZE + 1;

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
            continue;

        if (hasLiquid(chunk, xFrom, xTo, yFrom, yTo, zFrom, zTo, maxY))
            continue;

        _carveCanyonEllipsoid(chunk, x, y, z, horizontalRadius, verticalRadius, xFrom, xTo, yFrom, yTo, zFrom, zTo,
                              widthFactorPerHeight, minY, lavaLevel);
    }
}

std::vector<float> CanyonFeature::_initWidthFactors(IRandom &random, int32_t minY, int32_t maxY) {
    const int32_t depth = maxY - minY + 1;
    std::vector<float> widthFactorPerHeight((size_t) depth);
    float widthFactor = 1.0f;

    for (int32_t yIndex = 0; yIndex < depth; yIndex++) {
        if (yIndex == 0 || random.nextInt(3) == 0)
            widthFactor = 1.0f + random.nextFloat() * random.nextFloat();

        widthFactorPerHeight[(size_t) yIndex] = widthFactor * widthFactor;
    }

    return widthFactorPerHeight;
}

double CanyonFeature::_updateVerticalRadius(IRandom &random, double verticalRadius, int32_t distance,
                                            int32_t currentStep) {
    const float verticalMultiplier = 1.0f - std::fabs(0.5f - (float) currentStep / (float) distance) * 2.0f;
    const float factor = 1.0f + 0.0f * verticalMultiplier;
    return (double) factor * verticalRadius * (double) (0.75f + random.nextFloat() * 0.25f);
}

void CanyonFeature::_carveCanyonEllipsoid(LevelChunk &chunk, double x, double y, double z, double horizontalRadius,
                                          double verticalRadius, int32_t xFrom, int32_t xTo, int32_t yFrom,
                                          int32_t yTo, int32_t zFrom, int32_t zTo,
                                          const std::vector<float> &widthFactorPerHeight, int32_t minY,
                                          int32_t lavaLevel) {
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();
    const int32_t chunkXBlock = chunkX * CHUNK_SIZE;
    const int32_t chunkZBlock = chunkZ * CHUNK_SIZE;
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
                const int32_t yIndex = yy - minY;
                if (yIndex <= 0 || yIndex >= (int32_t) widthFactorPerHeight.size())
                    continue;

                const double yd = ((double) yy - 0.5 - y) * invVerticalRadius;
                const double shape = horizontalSq * (double) widthFactorPerHeight[(size_t) (yIndex - 1)]
                                     + yd * yd / 6.0;
                if (shape >= 1.0)
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
