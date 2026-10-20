#pragma once

#include "World/Level/LevelGen/Feature/IFeature.h"

class BlockManager;

class LakeFeature : public IFeature {
public:
    void apply(ChunkGenerateContext &context) override;

protected:
    virtual int32_t placementAttempts() const;

    virtual BlockState fluid() const = 0;

    virtual BlockState barrier() const;

    virtual bool canPlaceFeature(BlockManager &manager, int32_t x, int32_t y, int32_t z) const;

    virtual bool canReplaceWithAirOrFluid(BlockManager &manager, int32_t x, int32_t y, int32_t z) const;

    virtual bool canReplaceWithBarrier(BlockManager &manager, int32_t x, int32_t y, int32_t z) const;

    bool place(BlockManager &manager, int32_t originX, int32_t originY, int32_t originZ);

private:
    bool _validateBoundary(BlockManager &manager, const bool *grid, int32_t originX, int32_t originY, int32_t originZ,
                           const BlockState &fluidState) const;

    void _carveLake(BlockManager &manager, const bool *grid, int32_t originX, int32_t originY, int32_t originZ,
                    const BlockState &fluidState);

    void _placeBarrier(BlockManager &manager, const bool *grid, int32_t originX, int32_t originY, int32_t originZ);

    static bool _isBoundary(const bool *grid, int32_t xx, int32_t yy, int32_t zz);

    static int32_t _index(int32_t x, int32_t y, int32_t z);
};
