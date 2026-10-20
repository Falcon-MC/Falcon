#pragma once

#include "World/Level/LevelGen/Nether/Feature/NyliumVegetationFeature.h"

class WarpedVegetationFeature : public NyliumVegetationFeature {
public:
    int32_t getBiomeId() const override;

    const char *getNyliumIdentifier() const override;

    const BlockState &getFungusState() const override;

    const BlockState &getRootsState() const override;

    const char *name() const override;
};
