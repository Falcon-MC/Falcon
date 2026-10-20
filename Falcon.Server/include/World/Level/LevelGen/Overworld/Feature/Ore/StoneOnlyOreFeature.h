#pragma once

#include "World/Level/LevelGen/Feature/Ore/OreFeature.h"

class StoneOnlyOreFeature : public OreFeature {
public:
    bool canBeReplaced(const BlockState &state) const override;
};
