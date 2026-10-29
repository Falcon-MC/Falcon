#include "World/Level/LevelGen/Overworld/Feature/Decoration/CactusFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

namespace {

    const BlockState &cactusState() {
        static const BlockState state = VanillaBlocks::CACTUS().toBlockState();
        return state;
    }

    const BlockState &cactusFlowerState() {
        static const BlockState state = VanillaBlocks::CACTUS_FLOWER().toBlockState();
        return state;
    }

}

const char *CactusFeature::name() const {
    return "minecraft:desert_after_surface_cactus_feature_rules";
}

int32_t CactusFeature::getBase() const {
    return 2;
}

int32_t CactusFeature::getRandom() const {
    return 0;
}

void CactusFeature::populate(ChunkGenerateContext &context, IRandom &random) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();

    const int32_t x = random.nextBoundedInt(13) + 1;
    const int32_t z = random.nextBoundedInt(13) + 1;
    const int32_t y = chunk.getHeight(x, z) + 1;

    int32_t height = 1;
    const int32_t range = random.nextBoundedInt(18);
    if (range >= 16)
        height = 3;
    else if (range >= 11)
        height = 2;

    if (!DecorationSupport::isSand(chunk.getBlock(x, y - 1, z)))
        return;

    const int32_t worldX = x + (chunk.getX() << 4);
    const int32_t worldZ = z + (chunk.getZ() << 4);

    for (const DecorationSupport::FaceOffset &face: DecorationSupport::HORIZONTALS) {
        const BlockState *side = level.peekBlockPtr(worldX + face.mX, y + face.mY, worldZ + face.mZ);
        if (side == nullptr)
            return;

        if (!DecorationSupport::canBeReplaced(*side))
            return;
    }

    if (y <= 0)
        return;

    for (int32_t i = 0; i <= height; i++)
        chunk.setBlock(x, y + i, z, cactusState());

    if (random.nextBoolean())
        chunk.setBlock(x, y + height + 1, z, cactusFlowerState());
}
