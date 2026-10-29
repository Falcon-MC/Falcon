#pragma once

#include "World/Level/LevelGen/Overworld/Feature/Decoration/CoralFeature.h"

class CoralCrustFeature : public CoralFeature {
public:
    const char *name() const override;

    int32_t getBase() const override;

    int32_t getRandom() const override;

protected:
    bool placeFeature(LevelChunk &chunk, IRandom &random, int32_t x, int32_t y, int32_t z,
                      const BlockState &coralState) override;
};
