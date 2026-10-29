#pragma once

#include "World/Level/LevelGen/Overworld/Feature/River/DiscGenerateFeature.h"

class SandFeature : public DiscGenerateFeature {
public:
    const BlockState &getSourceBlock() const override;

    int32_t getMinRadius() const override;

    int32_t getMaxRadius() const override;

    int32_t getRadiusY() const override;

    int32_t getBase() const override;

    const char *name() const override;
};
