#pragma once

#include "World/Level/LevelGen/Overworld/Feature/Decoration/SeagrassFeature.h"

class WarmOceanSeagrassFeature : public SeagrassFeature {
public:
    const char *name() const override;

    int32_t getBase() const override;
};
