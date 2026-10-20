#pragma once

#include "World/Level/LevelGen/Feature/ObjectGenerator.h"

class EndPodiumObject : public ObjectGenerator {
public:
    explicit EndPodiumObject(bool active);

    ~EndPodiumObject() override = default;

    bool generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) override;

private:
    bool mActive;
};
