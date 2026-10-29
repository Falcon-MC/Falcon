#pragma once

#include "World/Level/LevelGen/Feature/IFeature.h"

/**
 * Approximates the ice caves `minecraft:noise_gradient` surface and subsurface builders, which vanilla applies
 * while building the terrain rather than as a feature. It recolours exposed stone, deepslate and tuff with the
 * biome's gradient ranges. Which blocks the vanilla builder replaces, how deep it goes and how it scales the
 * noise coordinates are not verified.
 */
class IceCavesSurfaceFeature : public IFeature {
public:
    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;
};
