#pragma once

#include "World/Level/Block/BlockState.h"
#include "World/Level/LevelGen/Feature/ObjectGenerator.h"

#include <string>

class BlockManager;

class VanillaTreeObject : public TreeGenerator {
public:
    static const int32_t TREE_WITH_VINES_CHANCE = 20;

    static bool canGrowInto(const std::string &identifier);

    static bool isLeaves(const std::string &identifier);

    static bool isSolid(const BlockState &state);

    static bool canBeReplaced(const BlockState &state);

    static bool isAir(const BlockState &state);

    static BlockState withPillarAxis(BlockState state, const char *axis);

    static BlockState withVineDirectionBits(BlockState state, int32_t bits);

    static BlockState getVineState(int32_t meta);

    static int32_t getMinHeight();

    static int32_t getMaxHeight();

protected:
    virtual void setDirtAt(BlockManager &manager, int32_t x, int32_t y, int32_t z);

    void addVinesAroundLog(BlockManager &manager, int32_t x, int32_t y, int32_t z);

private:
    void _addVine(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t meta);
};
