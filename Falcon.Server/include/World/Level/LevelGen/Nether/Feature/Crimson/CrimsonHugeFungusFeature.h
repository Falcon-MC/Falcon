#pragma once

#include "World/Level/LevelGen/Nether/Feature/HugeFungusFeature.h"

class CrimsonHugeFungusFeature : public HugeFungusFeature {
public:
    int32_t getBiomeId() const override;

    const char *getNyliumIdentifier() const override;

    std::shared_ptr<LegacyNetherTree> createTree(int32_t treeHeight) const override;

    const char *name() const override;
};
