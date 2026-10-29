#pragma once

#include "World/Level/LevelGen/Feature/IFeature.h"
#include "World/Level/LevelGen/Random/SimpleRandom.h"

#include <cstdint>
#include <set>

class BlockManager;

class IcebergFeature : public IFeature {
public:
    IcebergFeature();

    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;

private:
    void _generateBlueIce(BlockManager &manager, int32_t originX, int32_t originZ);

    void _generateCutOut(BlockManager &manager, int32_t width, int32_t height, int32_t originX, int32_t originY,
                         int32_t originZ, bool isEllipse, int32_t shapeEllipseA, double shapeAngle,
                         int32_t shapeEllipseC);

    void _carve(BlockManager &manager, int32_t radius, int32_t yOff, int32_t originX, int32_t originY,
                int32_t originZ, bool underWater, double angle, int32_t localOriginX, int32_t localOriginZ,
                int32_t shapeEllipseA, int32_t shapeEllipseC);

    void _removeFloatingSnowLayer(BlockManager &manager, int32_t x, int32_t y, int32_t z);

    void _generateIcebergBlock(BlockManager &manager, int32_t originX, int32_t originY, int32_t originZ,
                               int32_t height, int32_t xo, int32_t yOff, int32_t zo, int32_t radius, int32_t a,
                               bool isEllipse, int32_t shapeEllipseC, double shapeAngle, bool snowOnTop,
                               std::set<int64_t> &localIceberg);

    void _setIcebergBlock(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t hDiff, int32_t height,
                          bool isEllipse, bool snowOnTop, std::set<int64_t> &localIceberg);

    void _smooth(std::set<int64_t> &mask, BlockManager &manager, int32_t originX, int32_t originY, int32_t originZ,
                 int32_t width, int32_t height, bool isEllipse, int32_t shapeEllipseA);

    double _signedDistanceCircle(int32_t xo, int32_t zo, int32_t originX, int32_t originZ, int32_t radius);

    int32_t _heightDependentRadiusRound(int32_t yOff, int32_t height, int32_t width);

    int32_t _heightDependentRadiusEllipse(int32_t yOff, int32_t height, int32_t width);

    int32_t _heightDependentRadiusSteep(int32_t yOff, int32_t height, int32_t width);

    int32_t _nextIntExclusive(int32_t boundExclusive);

    SimpleRandom mLegacyRandom;
};
