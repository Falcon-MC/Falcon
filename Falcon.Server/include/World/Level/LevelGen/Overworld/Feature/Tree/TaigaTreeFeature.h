#pragma once

#include "World/Level/LevelGen/Feature/FeatureBases.h"

class TaigaTreeFeature : public GriddedFeature {
public:
    static const char *NAME;

    const char *name() const override;

    ObjectGeneratorPtr getGenerator(IRandom &random) override;

    bool canSpawnHere(int32_t biomeId) const override;

    void apply(ChunkGenerateContext &context) override;
};
