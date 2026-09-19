#pragma once

#include "Block/Block.h"
#include "Block/BlockState.h"
#include "Core/Math/Vector3i.h"

#include <string>

class ServerNetworkHandler;

class BaseRailBlock final : public Block {
public:
    explicit BaseRailBlock(const Block &base);

    static constexpr int STRAIGHT_NORTH_SOUTH = 0;
    static constexpr int STRAIGHT_EAST_WEST = 1;
    static constexpr int ASCENDING_EAST = 2;
    static constexpr int ASCENDING_WEST = 3;
    static constexpr int ASCENDING_NORTH = 4;
    static constexpr int ASCENDING_SOUTH = 5;
    static constexpr int CURVED_SOUTH_EAST = 6;
    static constexpr int CURVED_SOUTH_WEST = 7;
    static constexpr int CURVED_NORTH_WEST = 8;
    static constexpr int CURVED_NORTH_EAST = 9;

    static bool matches(const std::string &identifier);

    static bool isAbstract(const std::string &identifier);

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;

    void onPlacing(ServerNetworkHandler &owner, const Vector3i &position, BlockState &state) const override;
};
