#pragma once

#include "Level/Generator/Overworld/Feature/Decoration/DiscFeature.h"

class BushFeature : public DiscFeature {
public:
    const char *name() const override;

    BlockState getSourceBlock() const override;

    int32_t getMinRadius() const override;

    int32_t getMaxRadius() const override;

    double getProbability() const override;

    int32_t getBase() const override;

    int32_t getRandom() const override;

    bool isSupportValid(const BlockState &support, Level &level, int32_t x, int32_t y, int32_t z) const override;
};
