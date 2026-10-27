#pragma once

#include "World/Level/LevelGen/Overworld/Feature/Decoration/DiscFeature.h"

class DeadBushFeature : public DiscFeature {
public:
    const char *name() const override;

    BlockState getSourceBlock() const override;

    int32_t getMinRadius() const override;

    int32_t getMaxRadius() const override;

    double getProbability() const override;

    int32_t getBase() const override;

    int32_t getRandom() const override;
};
