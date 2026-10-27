#pragma once

#include "World/Level/LevelGen/Feature/IFeature.h"

class EndPodiumFeature : public IFeature {
public:
    EndPodiumFeature();

    ~EndPodiumFeature() override = default;

    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;
};
