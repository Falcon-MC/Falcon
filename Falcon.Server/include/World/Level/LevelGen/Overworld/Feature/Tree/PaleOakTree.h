#pragma once

#include "World/Level/LevelGen/Feature/Tree/VanillaTreeObject.h"

class PaleOakTree : public VanillaTreeObject {
public:
    bool generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) override;

    bool mTryCreakingHeart = false;

private:
    bool _placeTreeOfHeight(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t height);

    void _placeLogAt(BlockManager &manager, int32_t x, int32_t y, int32_t z, bool creaking);

    void _placeLeafAt(BlockManager &manager, int32_t x, int32_t y, int32_t z);
};
