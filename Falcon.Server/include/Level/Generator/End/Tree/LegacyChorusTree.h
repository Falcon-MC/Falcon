#pragma once

#include "Level/Generator/Feature/Tree/LegacyTreeObject.h"
#include "Core/Math/Vector3i.h"
#include <vector>

class LegacyChorusTree : public LegacyTreeObject {
public:
    bool generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) override;

    bool generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z, int32_t maxSize);

    void growImmediately(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z, int32_t maxSize,
                         int32_t age);

private:
    void _grow(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z,
               int32_t maxSize, int32_t age, std::vector<Vector3i> &plants);

    static void _connect(BlockManager &manager, const std::vector<Vector3i> &plants);

    static bool _isHorizontalAir(BlockManager &manager, int32_t x, int32_t y, int32_t z);

    static bool _isHorizontalAirExcept(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t exceptFace);
};
