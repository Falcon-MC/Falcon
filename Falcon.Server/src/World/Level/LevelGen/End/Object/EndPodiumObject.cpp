#include "World/Level/LevelGen/End/Object/EndPodiumObject.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Feature/Tree/TreeBlockFace.h"

namespace {

    const BlockState &bedrockState() {
        static const BlockState state = VanillaBlocks::BEDROCK().toBlockState();
        return state;
    }

    const BlockState &endStoneState() {
        static const BlockState state = VanillaBlocks::END_STONE().toBlockState();
        return state;
    }

    const BlockState &endPortalState() {
        static const BlockState state = VanillaBlocks::END_PORTAL().toBlockState();
        return state;
    }

    const BlockState &airState() {
        static const BlockState state = VanillaBlocks::AIR().toBlockState();
        return state;
    }

    const char *torchFacingFor(TreeBlockFace face) {
        switch (face) {
            case TreeBlockFace::NORTH:
                return "south";
            case TreeBlockFace::SOUTH:
                return "north";
            case TreeBlockFace::WEST:
                return "east";
            case TreeBlockFace::EAST:
                return "west";
            default:
                return "top";
        }
    }

    BlockState torchState(TreeBlockFace face) {
        BlockState base = VanillaBlocks::TORCH().toBlockState();
        base.mStates.putString("torch_facing_direction", torchFacingFor(face));
        return BlockState(base.mName, base.mStates);
    }

}

EndPodiumObject::EndPodiumObject(bool active)
        : mActive(active) {
}

bool EndPodiumObject::generate(BlockManager &manager, IRandom &, int32_t originX, int32_t originY, int32_t originZ) {
    for (int32_t x = -4; x <= 4; x++) {
        for (int32_t y = -1; y <= 32; y++) {
            for (int32_t z = -4; z <= 4; z++) {
                const int32_t distanceSquared = x * x + y * y + z * z;
                if (distanceSquared >= 13)
                    continue;

                if (y < 0) {
                    manager.setBlockStateAt(originX + x, originY + y, originZ + z,
                                            distanceSquared < 7 ? bedrockState() : endStoneState());
                } else if (y > 0) {
                    manager.setBlockStateAt(originX + x, originY + y, originZ + z, airState());
                } else if (distanceSquared >= 7) {
                    manager.setBlockStateAt(originX + x, originY, originZ + z, bedrockState());
                } else {
                    manager.setBlockStateAt(originX + x, originY, originZ + z,
                                            mActive ? endPortalState() : airState());
                }
            }
        }
    }

    for (int32_t y = 0; y < 4; y++)
        manager.setBlockStateAt(originX, originY + y, originZ, bedrockState());

    for (const TreeBlockFace face: TreeBlockFaces::HORIZONTALS) {
        manager.setBlockStateAt(originX + TreeBlockFaces::getXOffset(face), originY + 2,
                                originZ + TreeBlockFaces::getZOffset(face), torchState(face));
    }

    return true;
}
