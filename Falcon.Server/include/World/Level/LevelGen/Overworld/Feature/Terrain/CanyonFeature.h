#pragma once

#include "World/Level/LevelGen/Overworld/Feature/Terrain/CaveFeature.h"

#include <vector>

class CanyonFeature : public CaveFeature {
public:
    const char *name() const override;

protected:
    void carveChunk(IRandom &random, int32_t sourceChunkX, int32_t sourceChunkZ, LevelChunk &chunk) override;

private:
    void _carveCanyon(IRandom &random, LevelChunk &chunk, double x, double y, double z, float thickness,
                      float horizontalRotation, float verticalRotation, int32_t distance, int32_t minY,
                      int32_t maxY, int32_t lavaLevel);

    static std::vector<float> _initWidthFactors(IRandom &random, int32_t minY, int32_t maxY);

    static double _updateVerticalRadius(IRandom &random, double verticalRadius, int32_t distance,
                                        int32_t currentStep);

    void _carveCanyonEllipsoid(LevelChunk &chunk, double x, double y, double z, double horizontalRadius,
                               double verticalRadius, int32_t xFrom, int32_t xTo, int32_t yFrom, int32_t yTo,
                               int32_t zFrom, int32_t zTo, const std::vector<float> &widthFactorPerHeight,
                               int32_t minY, int32_t lavaLevel);
};
