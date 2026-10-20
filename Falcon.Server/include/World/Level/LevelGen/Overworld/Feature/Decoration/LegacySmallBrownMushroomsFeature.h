#pragma once

#include "World/Level/LevelGen/Overworld/Feature/Decoration/LegacySmallMushroomsFeature.h"

class LegacySmallBrownMushroomsFeature : public LegacySmallMushroomsFeature {
public:
    const char *name() const override;

    BlockState getSourceBlock() const override;

    int32_t getBase() const override;

    int32_t getRandom() const override;
};
