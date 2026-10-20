#pragma once

#include "World/Level/LevelGen/Feature/IFeature.h"
#include "World/Level/LevelGen/Feature/ObjectGenerator.h"

class BlockManager;

class AzaleaRootSystemFeature : public IFeature {
public:
    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;

    static void setGeneratorFactory(ObjectGeneratorPtr (*factory)());

private:
    bool _placeDirtAndTree(BlockManager &manager, int32_t originX, int32_t originY, int32_t originZ,
                           int32_t surfaceY);

    void _placeRootedDirtColumnDownward(BlockManager &manager, int32_t originX, int32_t originZ, int32_t startY,
                                        int32_t endY);

    void _placeRootedDirtLayer(BlockManager &manager, int32_t originX, int32_t y, int32_t originZ);

    void _placeHangingRoots(BlockManager &manager, int32_t originX, int32_t originY, int32_t originZ);
};
