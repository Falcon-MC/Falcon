#pragma once

#include "World/Level/LevelGen/Feature/FeatureBases.h"

class JunglePlantAggregateFeature : public MultiFeatureWrapper {
public:
    const char *name() const override;

    std::vector<const char *> getFeatures() const override;
};
