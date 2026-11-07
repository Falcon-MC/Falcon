#include "World/Level/LevelGen/Structure/StructureConnections.h"

#include "World/Level/Block/BlockSupport.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Block/FenceBlock.h"
#include "World/Level/Block/ThinFenceBlock.h"
#include "World/Level/Block/VanillaBlocks.h"

#include <array>

namespace {
    struct Side {
        int32_t mX;
        int32_t mZ;
    };

    // by weirdo_direction, the side a stair climbs to
    const std::array<Side, 4> STAIR_BACKS = {{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};

    bool isStair(const BlockState &state) {
        return state.mStates.contains("weirdo_direction");
    }

    // only the full back of a stair takes a post; a stair turned outwards has no full side left
    bool stairConnects(const BlockState &stair, int32_t sideX, int32_t sideZ) {
        const std::string corner = stair.mStates.getString("minecraft:corner", "none");
        if (corner == "outer_left" || corner == "outer_right")
            return false;

        const Side back = STAIR_BACKS[(size_t) stair.mStates.getInt("weirdo_direction", 0)];
        if (back.mX == -sideX && back.mZ == -sideZ)
            return true;

        if (corner == "none")
            return false;

        const Side turned = corner == "inner_left" ? Side{back.mZ, -back.mX} : Side{-back.mZ, back.mX};
        return turned.mX == -sideX && turned.mZ == -sideZ;
    }

    bool sameHalfStair(const BlockState &other, const BlockState &stair) {
        return isStair(other)
               && other.mStates.getByte("upside_down_bit", 0) == stair.mStates.getByte("upside_down_bit", 0);
    }

    // true when nothing on `side` continues the same run, so the stair there may turn towards it
    bool canTurn(const BlockState &stair, int32_t x, int32_t y, int32_t z, const Side &side,
                 const StructureConnections::Lookup &at) {
        const BlockState next = at(x + side.mX, y, z + side.mZ);
        return !sameHalfStair(next, stair)
               || next.mStates.getInt("weirdo_direction", 0) != stair.mStates.getInt("weirdo_direction", 0);
    }

    std::string stairCorner(const BlockState &stair, int32_t x, int32_t y, int32_t z,
                            const StructureConnections::Lookup &at) {
        const Side facing = STAIR_BACKS[(size_t) stair.mStates.getInt("weirdo_direction", 0)];
        const Side left = {facing.mZ, -facing.mX};
        const auto turned = [&](const BlockState &other, Side &side) {
            if (!sameHalfStair(other, stair))
                return false;
            side = STAIR_BACKS[(size_t) other.mStates.getInt("weirdo_direction", 0)];
            return side.mX * facing.mX + side.mZ * facing.mZ == 0;
        };

        Side side{};
        if (turned(at(x + facing.mX, y, z + facing.mZ), side) && canTurn(stair, x, y, z, {-side.mX, -side.mZ}, at))
            return side.mX == left.mX && side.mZ == left.mZ ? "outer_left" : "outer_right";

        if (turned(at(x - facing.mX, y, z - facing.mZ), side) && canTurn(stair, x, y, z, side, at))
            return side.mX == left.mX && side.mZ == left.mZ ? "inner_left" : "inner_right";

        return "none";
    }

    bool isThinOrFence(const BlockState &state) {
        return state.mStates.contains("minecraft:connection_east");
    }
}

bool StructureConnections::connects(const BlockState &neighbour, int32_t sideX, int32_t sideZ) {
    if (VanillaBlocks::getAs<FenceBlock>(neighbour.mName) != nullptr
        || VanillaBlocks::getAs<ThinFenceBlock>(neighbour.mName) != nullptr)
        return true;

    if (isStair(neighbour))
        return stairConnects(neighbour, sideX, sideZ);

    return BlockSupport::isSolid(neighbour) && !DecorationSupport::isTransparent(neighbour);
}

BlockState StructureConnections::connected(const BlockState &post, bool east, bool north, bool south, bool west) {
    Tag states = post.mStates;
    states.putByte("minecraft:connection_east", east ? 1 : 0);
    states.putByte("minecraft:connection_north", north ? 1 : 0);
    states.putByte("minecraft:connection_south", south ? 1 : 0);
    states.putByte("minecraft:connection_west", west ? 1 : 0);
    return BlockState(post.mName, states);
}

BlockState StructureConnections::shaped(const BlockState &block, int32_t x, int32_t y, int32_t z, const Lookup &at) {
    if (isThinOrFence(block))
        return connected(block, connects(at(x + 1, y, z), 1, 0), connects(at(x, y, z - 1), 0, -1),
                         connects(at(x, y, z + 1), 0, 1), connects(at(x - 1, y, z), -1, 0));

    if (isStair(block)) {
        Tag states = block.mStates;
        states.putString("minecraft:corner", stairCorner(block, x, y, z, at));
        return BlockState(block.mName, states);
    }

    return block;
}
