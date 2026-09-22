#include "Block/BlockSupport.h"

#include "Block/Block.h"
#include "Block/Blocks/FenceBlocks.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Components/PlacementOrientation.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"

namespace {
    const int FACE_OFFSETS[6][3] = {
            {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}
    };
}

namespace BlockSupport {
    Vector3i supportOf(const Vector3i &position, int blockFace) {
        if (blockFace < 0 || blockFace > 5)
            return Vector3i(position.x, position.y - 1, position.z);

        return Vector3i(position.x - FACE_OFFSETS[blockFace][0],
                        position.y - FACE_OFFSETS[blockFace][1],
                        position.z - FACE_OFFSETS[blockFace][2]);
    }

    bool isReplaceable(const BlockState &state) {
        if (DecorationSupport::isAir(state))
            return true;

        const Block *block = VanillaBlocks::fromIdentifier(state.mName);
        return block != nullptr && block->canBeReplaced(state);
    }

    bool isAttachable(const BlockState &support, int blockFace) {
        using PlacementOrientation::FACE_DOWN;
        using PlacementOrientation::FACE_UP;

        if (support.mName == "minecraft:farmland" || support.mName == "minecraft:grass_path")
            return blockFace == FACE_DOWN;

        if (blockFace == FACE_DOWN)
            return DecorationSupport::isSolid(support) && !DecorationSupport::isTransparent(support);

        if (DecorationSupport::isSolid(support))
            return true;

        if (VanillaBlocks::getAs<WallBlock>(support.mName) != nullptr
            || VanillaBlocks::getAs<FenceBlock>(support.mName) != nullptr)
            return blockFace == FACE_UP;

        return false;
    }

    bool isSolidOrCauldron(const BlockState &support) {
        return DecorationSupport::isSolid(support) || support.mName == "minecraft:cauldron";
    }
}
