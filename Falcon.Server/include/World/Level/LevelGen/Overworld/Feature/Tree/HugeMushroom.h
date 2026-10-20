#pragma once

#include "World/Level/LevelGen/Feature/ObjectGenerator.h"

class HugeMushroom : public ObjectGenerator {
public:
    enum class MushroomType : int32_t {
        RED = 0,
        BROWN = 1,
        RANDOM = 2
    };

    static const int32_t NORTH_WEST = 1;
    static const int32_t NORTH = 2;
    static const int32_t NORTH_EAST = 3;
    static const int32_t WEST = 4;
    static const int32_t CENTER = 5;
    static const int32_t EAST = 6;
    static const int32_t SOUTH_WEST = 7;
    static const int32_t SOUTH = 8;
    static const int32_t SOUTH_EAST = 9;
    static const int32_t STEM = 10;
    static const int32_t ALL_INSIDE = 0;

    HugeMushroom();

    explicit HugeMushroom(MushroomType mushroomType);

    bool generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) override;

private:
    MushroomType mMushroomType;
};
