#pragma once

#include "World/Level/LevelGen/Feature/ObjectGenerator.h"

class EndGatewayObject : public ObjectGenerator {
public:
    ~EndGatewayObject() override = default;

    bool generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) override;
};
