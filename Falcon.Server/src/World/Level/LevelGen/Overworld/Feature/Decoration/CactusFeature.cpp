#include "World/Level/LevelGen/Overworld/Feature/Decoration/CactusFeature.h"

#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"

namespace {

    const BlockState &cactusState() {
        static const BlockState state = VanillaBlocks::CACTUS().toBlockState();
        return state;
    }

    const BlockState &cactusFlowerState() {
        static const BlockState state = VanillaBlocks::CACTUS_FLOWER().toBlockState();
        return state;
    }

    // the cactus' own placement: on sand or cactus, nothing solid beside it
    bool cactusSurvives(BlockManager &blocks, int32_t x, int32_t y, int32_t z) {
        const BlockState &below = blocks.getBlockAt(x, y - 1, z);
        if (!DecorationSupport::isSand(below) && below.mName != "minecraft:cactus")
            return false;

        for (const DecorationSupport::FaceOffset &face: DecorationSupport::HORIZONTALS) {
            if (BlockSupport::isSolid(blocks.getBlockAt(x + face.mX, y, z + face.mZ)))
                return false;
        }
        return true;
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

bool CactusFeature::placeCluster(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random) {
    // ten tries spread by raw draws, z, y then x; a stack of 1 to 3, a flower on one in three
    for (int32_t attempt = 0; attempt < 10; attempt++) {
        const int32_t offsetZ = static_cast<int32_t>(random() & 7u) - static_cast<int32_t>(random() & 7u);
        const int32_t offsetY = static_cast<int32_t>(random() & 3u) - static_cast<int32_t>(random() & 3u);
        const int32_t offsetX = static_cast<int32_t>(random() & 7u) - static_cast<int32_t>(random() & 7u);
        const int32_t atX = x + offsetX;
        const int32_t atY = y + offsetY;
        const int32_t atZ = z + offsetZ;
        if (!DecorationSupport::isAir(blocks.getBlockAt(atX, atY, atZ)))
            continue;

        const int32_t height = FeatureRandom::nextInt(random, FeatureRandom::nextInt(random, 3) + 1) + 1;
        int32_t placed = 0;
        while (placed < height && cactusSurvives(blocks, atX, atY + placed, atZ)) {
            blocks.setBlockStateAt(atX, atY + placed, atZ, cactusState());
            placed++;
        }
        if (placed == 0 || FeatureRandom::nextInt(random, 3) != 0)
            continue;
        if (DecorationSupport::isAir(blocks.getBlockAt(atX, atY + placed, atZ)))
            blocks.setBlockStateAt(atX, atY + placed, atZ, cactusFlowerState());
    }
    return true;
}
