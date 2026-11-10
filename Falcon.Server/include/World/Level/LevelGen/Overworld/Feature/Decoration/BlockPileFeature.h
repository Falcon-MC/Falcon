#pragma once

#include <cstdint>
#include <random>

class BlockManager;

// the village piles of hay, melons, pumpkins, snow and ice (vtable 0x14F2F01D0, registered in sub_14726F340)
class BlockPileFeature {
public:
    enum class Pile { Hay, Melon, Pumpkin, Snow, Ice };

    static bool placeHay(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);

    static bool placeMelon(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);

    static bool placePumpkin(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);

    static bool placeSnow(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);

    static bool placeIce(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);

private:
    static bool _place(BlockManager &blocks, Pile pile, int32_t x, int32_t y, int32_t z, std::mt19937 &random);
};
