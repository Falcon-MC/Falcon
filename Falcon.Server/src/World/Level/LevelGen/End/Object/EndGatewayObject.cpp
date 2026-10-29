#include "World/Level/LevelGen/End/Object/EndGatewayObject.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Feature/Tree/TreeBlockFace.h"

namespace {

    const TreeBlockFace ALL_FACES[6] = {
            TreeBlockFace::DOWN,
            TreeBlockFace::UP,
            TreeBlockFace::NORTH,
            TreeBlockFace::SOUTH,
            TreeBlockFace::WEST,
            TreeBlockFace::EAST
    };

    const BlockState &bedrockState() {
        static const BlockState state = VanillaBlocks::BEDROCK().toBlockState();
        return state;
    }

    const BlockState &endGatewayState() {
        static const BlockState state = VanillaBlocks::END_GATEWAY().toBlockState();
        return state;
    }

}

bool EndGatewayObject::generate(BlockManager &manager, IRandom &, int32_t x, int32_t y, int32_t z) {
    for (const TreeBlockFace face: ALL_FACES) {
        manager.setBlockStateAt(x + TreeBlockFaces::getXOffset(face),
                                y + 1 + TreeBlockFaces::getYOffset(face),
                                z + TreeBlockFaces::getZOffset(face), bedrockState());
    }

    for (const TreeBlockFace face: ALL_FACES) {
        manager.setBlockStateAt(x + TreeBlockFaces::getXOffset(face),
                                y - 1 + TreeBlockFaces::getYOffset(face),
                                z + TreeBlockFaces::getZOffset(face), bedrockState());
    }

    manager.setBlockStateAt(x, y, z, endGatewayState());
    return true;
}
