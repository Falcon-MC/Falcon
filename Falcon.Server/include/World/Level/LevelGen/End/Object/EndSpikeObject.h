#pragma once

#include "World/Level/LevelGen/Feature/ObjectGenerator.h"

class EndSpikeObject : public ObjectGenerator {
public:
    EndSpikeObject(int32_t radius, int32_t height, bool guarded);

    ~EndSpikeObject() override = default;

    bool generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) override;

private:
    int32_t mRadius;
    int32_t mHeight;
    bool mGuarded;
};
