#include "World/Level/LevelGen/Overworld/Feature/Decoration/SulfurSpringTrailToSurfaceFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <string>

namespace {

    const int32_t REQUIRED_VERTICAL_SPACE_FOR_TREE = 5;
    const int32_t LEVEL_TEST_DISTANCE = 8;
    const int32_t MAX_LEVEL_DEVIATION = 2;
    const int32_t ROOT_RADIUS = 3;
    const int32_t ROOT_PLACEMENT_ATTEMPTS = 20;
    const int32_t ROOT_COLUMN_MAX_HEIGHT = 184;
    const int32_t HANGING_ROOT_PLACEMENT_ATTEMPTS = 1;
    const int32_t HANGING_ROOT_RADIUS = 1;
    const int32_t HANGING_ROOT_VERTICAL_SPAN = 1;
    const int32_t ALLOWED_VERTICAL_WATER_FOR_TREE = 1;
    const int32_t MAX_CEILING_SCAN_STEPS = 12;
    const int32_t TEMPLATE_Y_OFFSET = -7;

    struct SpringVariant {
        int32_t mWeight;
        int32_t mTuffCount;
        int32_t mTuffSpread;
        int32_t mTemplateCount;
    };

    const SpringVariant SPRING_VARIANTS[4] = {
            {200, 64, 7, 4},
            {90,  80, 8, 3},
            {20,  96, 9, 2},
            {5,   128, 10, 1}
    };

    const BlockState &sulfurState() {
        static const BlockState state = VanillaBlocks::SULFUR().toBlockState();
        return state;
    }

    const BlockState &tuffState() {
        static const BlockState state = VanillaBlocks::TUFF().toBlockState();
        return state;
    }

    bool isRootReplaceable(const std::string &id) {
        return id == "minecraft:stone"
               || id == "minecraft:deepslate"
               || id == "minecraft:tuff"
               || id == "minecraft:dirt"
               || id == "minecraft:clay"
               || id == "minecraft:moss_block"
               || id == "minecraft:dirt_with_roots"
               || id == "minecraft:sulfur"
               || id == "minecraft:cinnabar";
    }

}

const char *SulfurSpringTrailToSurfaceFeature::name() const {
    return "minecraft:sulfur_spring_trail_to_surface_snap_to_ceiling_feature";
}

int32_t SulfurSpringTrailToSurfaceFeature::_findCeilingAnchor(BlockManager &manager, int32_t x, int32_t y, int32_t z,
                                                              int32_t minY, int32_t maxY) {
    for (int32_t step = 0; step <= MAX_CEILING_SCAN_STEPS && y + step <= maxY; step++) {
        const int32_t scanY = y + step;
        const BlockState block = manager.getBlockAt(x, scanY, z);
        if (DecorationSupport::isSolid(block)) {
            const int32_t anchorY = scanY - 1;
            if (anchorY < minY)
                return std::numeric_limits<int32_t>::min();

            return DecorationSupport::isAir(manager.getBlockAt(x, anchorY, z))
                   ? anchorY
                   : std::numeric_limits<int32_t>::min();
        }

        if (!DecorationSupport::isAir(block))
            return std::numeric_limits<int32_t>::min();
    }

    return std::numeric_limits<int32_t>::min();
}

bool SulfurSpringTrailToSurfaceFeature::_hasRequiredVerticalSpace(BlockManager &manager, int32_t x, int32_t y,
                                                                  int32_t z) {
    int32_t water = 0;

    for (int32_t i = 1; i <= REQUIRED_VERTICAL_SPACE_FOR_TREE; i++) {
        const BlockState block = manager.getBlockAt(x, y + i, z);
        if (DecorationSupport::isAir(block))
            continue;

        if (DecorationSupport::isWater(block) && ++water <= ALLOWED_VERTICAL_WATER_FOR_TREE)
            continue;

        return false;
    }

    return true;
}

int32_t SulfurSpringTrailToSurfaceFeature::_findTopSolidY(BlockManager &manager, int32_t x, int32_t startY,
                                                          int32_t z) {
    for (int32_t offset = 0; offset <= MAX_LEVEL_DEVIATION; offset++) {
        if (DecorationSupport::isSolid(manager.getBlockAt(x, startY + offset, z)))
            return startY + offset;

        if (DecorationSupport::isSolid(manager.getBlockAt(x, startY - offset, z)))
            return startY - offset;
    }

    return std::numeric_limits<int32_t>::min() / 2;
}

bool SulfurSpringTrailToSurfaceFeature::_passesLevelTest(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    const int32_t baseY = y - 1;
    return std::abs(_findTopSolidY(manager, x + LEVEL_TEST_DISTANCE, baseY, z) - baseY) <= MAX_LEVEL_DEVIATION
           && std::abs(_findTopSolidY(manager, x - LEVEL_TEST_DISTANCE, baseY, z) - baseY) <= MAX_LEVEL_DEVIATION
           && std::abs(_findTopSolidY(manager, x, baseY, z + LEVEL_TEST_DISTANCE) - baseY) <= MAX_LEVEL_DEVIATION
           && std::abs(_findTopSolidY(manager, x, baseY, z - LEVEL_TEST_DISTANCE) - baseY) <= MAX_LEVEL_DEVIATION;
}

bool SulfurSpringTrailToSurfaceFeature::_hasSolidNonLavaBelow(BlockManager &manager, int32_t x, int32_t y,
                                                              int32_t z) {
    const BlockState below = manager.getBlockAt(x, y - 1, z);
    return DecorationSupport::isSolid(below)
           && below.mName != "minecraft:lava"
           && below.mName != "minecraft:flowing_lava";
}

int32_t SulfurSpringTrailToSurfaceFeature::_findAllowedTreePosition(BlockManager &manager, int32_t x, int32_t originY,
                                                                    int32_t z, int32_t minY, int32_t maxY) {
    const int32_t endY = std::min(originY + ROOT_COLUMN_MAX_HEIGHT, maxY);

    for (int32_t y = std::max(originY, minY + 1); y <= endY; y++) {
        if (DecorationSupport::isAir(manager.getBlockAt(x, y, z))
            && _hasRequiredVerticalSpace(manager, x, y, z)
            && _passesLevelTest(manager, x, y, z)
            && _hasSolidNonLavaBelow(manager, x, y, z))
            return y;
    }

    return std::numeric_limits<int32_t>::min();
}

void SulfurSpringTrailToSurfaceFeature::_placeRootColumn(BlockManager &manager, int32_t originX, int32_t originZ,
                                                         int32_t startY, int32_t endY) {
    const int32_t min = std::min(startY, endY);
    const int32_t max = std::max(startY, endY);

    for (int32_t y = min; y <= max; y++) {
        for (int32_t i = 0; i < ROOT_PLACEMENT_ATTEMPTS; i++) {
            const int32_t x = originX + mRandom.nextInt(ROOT_RADIUS) - mRandom.nextInt(ROOT_RADIUS);
            const int32_t z = originZ + mRandom.nextInt(ROOT_RADIUS) - mRandom.nextInt(ROOT_RADIUS);
            if (isRootReplaceable(manager.getBlockAt(x, y, z).mName))
                manager.setBlockStateAt(x, y, z, sulfurState());
        }
    }
}

void SulfurSpringTrailToSurfaceFeature::_placeHangingRoots(BlockManager &manager, int32_t originX, int32_t originY,
                                                           int32_t originZ) {
    for (int32_t i = 0; i < HANGING_ROOT_PLACEMENT_ATTEMPTS; i++) {
        const int32_t x = originX + mRandom.nextInt(HANGING_ROOT_RADIUS) - mRandom.nextInt(HANGING_ROOT_RADIUS);
        const int32_t y = originY + mRandom.nextInt(HANGING_ROOT_VERTICAL_SPAN)
                          - mRandom.nextInt(HANGING_ROOT_VERTICAL_SPAN);
        const int32_t z = originZ + mRandom.nextInt(HANGING_ROOT_RADIUS) - mRandom.nextInt(HANGING_ROOT_RADIUS);

        if (DecorationSupport::isAir(manager.getBlockAt(x, y, z))
            && DecorationSupport::isSolid(manager.getBlockAt(x, y + 1, z)))
            manager.setBlockStateAt(x, y, z, sulfurState());
    }
}

int32_t SulfurSpringTrailToSurfaceFeature::_triangle(int32_t spread) {
    return mRandom.nextInt(spread) - mRandom.nextInt(spread);
}

int32_t SulfurSpringTrailToSurfaceFeature::_scanDownToSolid(BlockManager &manager, int32_t x, int32_t y, int32_t z,
                                                            int32_t minY, int32_t maxSteps) {
    for (int32_t step = 0; step <= maxSteps && y - step >= minY; step++) {
        const int32_t scanY = y - step;
        if (DecorationSupport::isSolid(manager.getBlockAt(x, scanY, z)))
            return scanY;
    }

    return std::numeric_limits<int32_t>::min();
}

void SulfurSpringTrailToSurfaceFeature::_placeTuffCover(BlockManager &manager, int32_t originX, int32_t originY,
                                                        int32_t originZ, int32_t minY, int32_t count,
                                                        int32_t spread) {
    for (int32_t i = 0; i < count; i++) {
        const int32_t x = originX + _triangle(spread);
        const int32_t y = originY + _triangle(3);
        const int32_t z = originZ + _triangle(spread);
        const int32_t targetY = _scanDownToSolid(manager, x, y, z, minY, 4);
        if (targetY != std::numeric_limits<int32_t>::min()
            && DecorationSupport::isSolid(manager.getBlockAt(x, targetY, z)))
            manager.setBlockStateAt(x, targetY, z, tuffState());
    }
}

void SulfurSpringTrailToSurfaceFeature::_placeSulfurSpring(BlockManager &manager, int32_t originX, int32_t originY,
                                                           int32_t originZ, int32_t minY) {
    int32_t totalWeight = 0;
    for (const SpringVariant &variant: SPRING_VARIANTS)
        totalWeight += variant.mWeight;

    const SpringVariant *chosen = &SPRING_VARIANTS[0];
    int32_t value = mRandom.nextInt(totalWeight - 1);
    for (const SpringVariant &variant: SPRING_VARIANTS) {
        value -= variant.mWeight;
        if (value < 0) {
            chosen = &variant;
            break;
        }
    }

    _placeTuffCover(manager, originX, originY, originZ, minY, chosen->mTuffCount, chosen->mTuffSpread);

    const int32_t templateY = originY + TEMPLATE_Y_OFFSET;
    if (templateY >= minY)
        mRandom.nextInt(chosen->mTemplateCount - 1);
}

bool SulfurSpringTrailToSurfaceFeature::_placeRootedSpring(BlockManager &manager, int32_t originX, int32_t originY,
                                                           int32_t originZ, int32_t minY, int32_t maxY) {
    if (!DecorationSupport::isAir(manager.getBlockAt(originX, originY, originZ)))
        return false;

    const int32_t chosenY = _findAllowedTreePosition(manager, originX, originY, originZ, minY, maxY);
    if (chosenY == std::numeric_limits<int32_t>::min())
        return false;

    _placeSulfurSpring(manager, originX, chosenY, originZ, minY);
    _placeRootColumn(manager, originX, originZ, originY, chosenY);
    _placeHangingRoots(manager, originX, originY, originZ);
    return true;
}

void SulfurSpringTrailToSurfaceFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    mRandom.setSeed(level.getSeed() ^ DecorationSupport::chunkHash(chunkX, chunkZ) ^ javaStringHash(name()));

    BlockManager manager(level);
    const int32_t minY = LevelChunk::MIN_Y;
    const int32_t maxY = std::min(256, (int32_t) LevelChunk::MAX_Y);
    const int32_t count = mRandom.nextInt(1, 2);
    bool placed = false;

    for (int32_t i = 0; i < count; i++) {
        const int32_t x = (chunkX << 4) + mRandom.nextBoundedInt(15);
        const int32_t z = (chunkZ << 4) + mRandom.nextBoundedInt(15);
        const int32_t y = mRandom.nextInt(minY, maxY);

        const int32_t originY = _findCeilingAnchor(manager, x, y, z, minY, maxY);
        if (originY < minY || originY > maxY)
            continue;

        if (_placeRootedSpring(manager, x, originY, z, minY, maxY))
            placed = true;
    }

    if (placed)
        queueObject(manager);
}
