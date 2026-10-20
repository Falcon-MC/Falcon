#pragma once

#include "World/Level/LevelGen/Random/IRandom.h"

#include <cstdint>
#include <memory>

class BlockManager;

class ObjectGenerator {
public:
    virtual ~ObjectGenerator() = default;

    virtual bool generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) = 0;
};

class TreeGenerator : public ObjectGenerator {
public:
    ~TreeGenerator() override = default;
};

using ObjectGeneratorPtr = std::shared_ptr<ObjectGenerator>;
using TreeGeneratorPtr = std::shared_ptr<TreeGenerator>;
