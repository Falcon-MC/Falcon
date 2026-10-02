#include "Level/Generator/Overworld/Feature/Decoration/MultifaceFeature.h"

#include "Block/Blocks/VanillaBlocks.h"
#include "Level/Generator/Biome/BiomeChunkGenDataRegistry.h"
#include "Level/Generator/Feature/BlockManager.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"

#include <algorithm>

namespace {

    const int32_t MAX_ABSOLUTE_Y = 256;
    const int32_t MAX_SURFACE_RELATIVE_Y = -13;
    const int32_t NO_FACE = -1;
    const char *const NO_GLOW_LICHEN_TAG = "no_glow_lichen";

    const int32_t DUSWNE_INDEX[6] = {0, 1, 4, 2, 3, 5};

    const int32_t EDGES[6][4] = {
            {2, 3, 4, 5},
            {2, 3, 4, 5},
            {0, 1, 4, 5},
            {0, 1, 4, 5},
            {0, 1, 2, 3},
            {0, 1, 2, 3}
    };

    const BlockState &glowLichenState() {
        static const BlockState state = VanillaBlocks::GLOW_LICHEN().toBlockState();
        return state;
    }

    int32_t lichenBits(const BlockState &state) {
        if (state.mName != "minecraft:glow_lichen")
            return 0;

        const Tag *tag = state.mStates.get("multi_face_direction_bits");
        if (tag == nullptr)
            return 0;

        return tag->asInt();
    }

}

const char *MultifaceFeature::name() const {
    return "minecraft:underground_glow_lichen_feature";
}

int32_t MultifaceFeature::getBase() const {
    return 52;
}

int32_t MultifaceFeature::getRandom() const {
    return 26;
}

int32_t MultifaceFeature::_getOceanFloorHeight(BlockManager &manager, int32_t x, int32_t z, int32_t minY,
                                               int32_t maxY) {
    for (int32_t y = maxY; y >= minY; y--) {
        const BlockState state = manager.getBlockAt(x, y, z);
        if (!DecorationSupport::isAir(state) && !DecorationSupport::isLiquid(state))
            return y;
    }

    return minY;
}

int32_t MultifaceFeature::_placeAt(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random,
                                   int32_t preferredFace) {
    const BlockState block = manager.getBlockAt(x, y, z);
    if (!DecorationSupport::isAir(block) && block.mName != "minecraft:glow_lichen")
        return NO_FACE;

    const int32_t currentBits = lichenBits(block);
    const int32_t startIndex = preferredFace == NO_FACE ? random.nextInt(5) : preferredFace;

    for (int32_t i = 0; i < 6; i++) {
        const int32_t faceIndex = (startIndex + i) % 6;
        const DecorationSupport::FaceOffset &face = DecorationSupport::ALL_FACES[faceIndex];
        const BlockState support = manager.getBlockAt(x + face.mX, y + face.mY, z + face.mZ);
        if (!DecorationSupport::isSolid(support))
            continue;

        const int32_t bit = 1 << DUSWNE_INDEX[faceIndex];
        if ((currentBits & bit) != 0)
            continue;

        manager.setBlockStateAt(x, y, z,
                                DecorationSupport::withState(glowLichenState(), "multi_face_direction_bits",
                                                             currentBits | bit));
        return faceIndex;
    }

    return NO_FACE;
}

void MultifaceFeature::populate(ChunkGenerateContext &context, IRandom &random) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();

    const int32_t minY = LevelChunk::MIN_Y;
    const int32_t maxY = std::min(MAX_ABSOLUTE_Y, (int32_t) LevelChunk::MAX_Y);
    if (minY > maxY)
        return;

    BlockManager manager(level);
    const int32_t x = (chunk.getX() << 4) + random.nextInt(14) + 1;
    const int32_t z = (chunk.getZ() << 4) + random.nextInt(14) + 1;
    const int32_t y = random.nextInt(minY, maxY);

    const int32_t biomeId = (int32_t) chunk.getBiomeAt(x & 0x0f, y, z & 0x0f);
    if (BiomeChunkGenDataRegistry::hasTag(biomeId, NO_GLOW_LICHEN_TAG))
        return;

    if (y > _getOceanFloorHeight(manager, x, z, minY, maxY) + MAX_SURFACE_RELATIVE_Y)
        return;

    const int32_t face = _placeAt(manager, x, y, z, random, NO_FACE);
    if (face == NO_FACE)
        return;

    int32_t placed = 1;
    const int32_t target = 1 + random.nextInt(3);
    int32_t currentX = x;
    int32_t currentY = y;
    int32_t currentZ = z;
    int32_t currentFace = face;

    for (int32_t attempts = 0; attempts < 8 && placed < target; attempts++) {
        const int32_t edge = EDGES[currentFace][random.nextInt(3)];
        const DecorationSupport::FaceOffset &edgeOffset = DecorationSupport::ALL_FACES[edge];
        const int32_t nextX = currentX + edgeOffset.mX;
        const int32_t nextY = currentY + edgeOffset.mY;
        const int32_t nextZ = currentZ + edgeOffset.mZ;
        const int32_t nextFace = _placeAt(manager, nextX, nextY, nextZ, random, currentFace);
        if (nextFace == NO_FACE)
            continue;

        placed++;
        currentX = nextX;
        currentY = nextY;
        currentZ = nextZ;
        currentFace = nextFace;
    }

    queueObject(manager);
}
