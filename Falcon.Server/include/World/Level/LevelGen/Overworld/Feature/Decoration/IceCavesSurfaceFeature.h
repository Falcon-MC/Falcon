#pragma once

#include "World/Level/LevelGen/Feature/IFeature.h"

// approximates the ice caves `minecraft:noise_gradient` surface builders (terrain-time in the game): recolours
// exposed stone, deepslate and tuff; replaced blocks, depth and noise scale unverified
class IceCavesSurfaceFeature : public IFeature {
public:
    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;
};
