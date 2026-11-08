#include "World/Level/LevelGen/Structure/StructureConnections.h"

#include "World/Level/Block/BannerBlock.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Block/FenceBlock.h"
#include "World/Level/Block/LanternBlock.h"
#include "World/Level/Block/PressurePlateBlock.h"
#include "World/Level/Block/SignBlock.h"
#include "World/Level/Block/ThinFenceBlock.h"
#include "World/Level/Block/TorchBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/WallBannerBlock.h"
#include "World/Level/Block/WallSignBlock.h"

#include <array>
#include <string>

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

    // fences take a gate across the side; panes do not
    bool gateConnects(const BlockState &post, const BlockState &neighbour, int32_t sideX, int32_t sideZ) {
        if (VanillaBlocks::getAs<FenceBlock>(post.mName) == nullptr || !neighbour.mStates.contains("in_wall_bit"))
            return false;

        const std::string facing = neighbour.mStates.getString("minecraft:cardinal_direction", "south");
        const bool alongX = facing == "east" || facing == "west";
        return sideX != 0 ? !alongX : alongX;
    }

    bool postConnects(const BlockState &post, const BlockState &neighbour, int32_t sideX, int32_t sideZ) {
        return gateConnects(post, neighbour, sideX, sideZ) || StructureConnections::connects(neighbour, sideX, sideZ);
    }

    bool isWall(const BlockState &state) {
        return state.mStates.contains("wall_connection_type_east");
    }

    bool isFullSolid(const BlockState &state) {
        return BlockSupport::isSolid(state) && !DecorationSupport::isTransparent(state) && !isWall(state);
    }

    // walls meet walls, panes, bars, gates across the side and full faces, never fences
    bool wallConnects(const BlockState &neighbour, int32_t sideX, int32_t sideZ) {
        if (isWall(neighbour) || VanillaBlocks::getAs<ThinFenceBlock>(neighbour.mName) != nullptr)
            return true;
        if (neighbour.mStates.contains("in_wall_bit")) {
            const std::string facing = neighbour.mStates.getString("minecraft:cardinal_direction", "south");
            const bool alongX = facing == "east" || facing == "west";
            return sideX != 0 ? !alongX : alongX;
        }
        return isFullSolid(neighbour) || (isStair(neighbour) && stairConnects(neighbour, sideX, sideZ));
    }

    // a side rises when the block above covers it: a full block or a wall reaching the same side
    const char *wallSide(bool connected, const BlockState &above, bool aboveReaches) {
        if (!connected)
            return "none";
        return isFullSolid(above) || (isWall(above) && aboveReaches) ? "tall" : "short";
    }

    // torches, lanterns, signs, banners and pressure plates keep the post under them
    bool raisesPost(const BlockState &above) {
        const Block *block = VanillaBlocks::fromIdentifier(above.mName);
        if (dynamic_cast<const TorchBlock *>(block) != nullptr || dynamic_cast<const LanternBlock *>(block) != nullptr
            || dynamic_cast<const SignBlock *>(block) != nullptr || dynamic_cast<const WallSignBlock *>(block) != nullptr
            || dynamic_cast<const BannerBlock *>(block) != nullptr
            || dynamic_cast<const WallBannerBlock *>(block) != nullptr
            || dynamic_cast<const PressurePlateBlock *>(block) != nullptr)
            return true;
        return isWall(above) || isFullSolid(above);
    }

    BlockState shapedWall(const BlockState &wall, int32_t x, int32_t y, int32_t z,
                          const StructureConnections::Lookup &at) {
        static const std::array<Side, 4> SIDES = {{{1, 0}, {0, -1}, {0, 1}, {-1, 0}}};
        static const char *KEYS[] = {"wall_connection_type_east", "wall_connection_type_north",
                                     "wall_connection_type_south", "wall_connection_type_west"};
        const BlockState above = at(x, y + 1, z);
        std::array<const char *, 4> sides{};
        for (size_t i = 0; i < SIDES.size(); ++i) {
            const Side &side = SIDES[i];
            const bool connected = wallConnects(at(x + side.mX, y, z + side.mZ), side.mX, side.mZ);
            const bool aboveReaches = isWall(above)
                                      && wallConnects(at(x + side.mX, y + 1, z + side.mZ), side.mX, side.mZ);
            sides[i] = wallSide(connected, above, aboveReaches);
        }

        // a lone post or an uneven pair keeps the post, two tall opposite sides drop it, else the block above decides
        const auto none = [&](size_t i) {
            return std::string(sides[i]) == "none";
        };
        const auto tall = [&](size_t i) {
            return std::string(sides[i]) == "tall";
        };
        bool post = true;
        const bool alone = none(0) && none(1) && none(2) && none(3);
        if (!alone && none(1) == none(2) && none(0) == none(3)) {
            const bool tallRun = (tall(1) && tall(2)) || (tall(0) && tall(3));
            post = !tallRun && raisesPost(above);
        }

        Tag states = wall.mStates;
        for (size_t i = 0; i < SIDES.size(); ++i)
            states.putString(KEYS[i], sides[i]);
        states.putByte("wall_post_bit", post ? 1 : 0);
        return BlockState(wall.mName, states);
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
        return connected(block, postConnects(block, at(x + 1, y, z), 1, 0),
                         postConnects(block, at(x, y, z - 1), 0, -1), postConnects(block, at(x, y, z + 1), 0, 1),
                         postConnects(block, at(x - 1, y, z), -1, 0));

    if (isWall(block))
        return shapedWall(block, x, y, z, at);

    if (isStair(block)) {
        Tag states = block.mStates;
        states.putString("minecraft:corner", stairCorner(block, x, y, z, at));
        return BlockState(block.mName, states);
    }

    return block;
}
