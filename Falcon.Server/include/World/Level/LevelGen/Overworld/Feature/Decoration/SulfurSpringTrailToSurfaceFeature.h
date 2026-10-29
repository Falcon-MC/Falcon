#pragma once

#include "World/Level/LevelGen/Feature/IFeature.h"

class BlockManager;

class SulfurSpringTrailToSurfaceFeature : public IFeature {
public:
    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;

private:
    static int32_t _findCeilingAnchor(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t minY,
                                      int32_t maxY);

    bool _placeRootedSpring(BlockManager &manager, int32_t originX, int32_t originY, int32_t originZ, int32_t minY,
                            int32_t maxY);

    int32_t _findAllowedTreePosition(BlockManager &manager, int32_t x, int32_t originY, int32_t z, int32_t minY,
                                     int32_t maxY);

    void _placeRootColumn(BlockManager &manager, int32_t originX, int32_t originZ, int32_t startY, int32_t endY);

    void _placeHangingRoots(BlockManager &manager, int32_t originX, int32_t originY, int32_t originZ);

    void _placeSulfurSpring(BlockManager &manager, int32_t originX, int32_t originY, int32_t originZ, int32_t minY);

    void _placeTuffCover(BlockManager &manager, int32_t originX, int32_t originY, int32_t originZ, int32_t minY,
                         int32_t count, int32_t spread);

    static int32_t _scanDownToSolid(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t minY,
                                    int32_t maxSteps);

    int32_t _triangle(int32_t spread);

    static bool _hasRequiredVerticalSpace(BlockManager &manager, int32_t x, int32_t y, int32_t z);

    static bool _passesLevelTest(BlockManager &manager, int32_t x, int32_t y, int32_t z);

    static int32_t _findTopSolidY(BlockManager &manager, int32_t x, int32_t startY, int32_t z);

    static bool _hasSolidNonLavaBelow(BlockManager &manager, int32_t x, int32_t y, int32_t z);
};
