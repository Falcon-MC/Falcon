#include "Level/Generator/Overworld/Feature/Decoration/GeodeFeature.h"

#include "Block/Blocks/VanillaBlocks.h"
#include "Core/Math/MathConstants.h"
#include "Level/Generator/Feature/BlockManager.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"

#include <algorithm>
#include <cmath>

namespace {

    const BlockState &smoothBasaltState() {
        static const BlockState state = VanillaBlocks::SMOOTH_BASALT().toBlockState();
        return state;
    }

    const BlockState &calciteState() {
        static const BlockState state = VanillaBlocks::CALCITE().toBlockState();
        return state;
    }

    const BlockState &amethystBlockState() {
        static const BlockState state = VanillaBlocks::AMETHYST_BLOCK().toBlockState();
        return state;
    }

    const BlockState &buddingAmethystState() {
        static const BlockState state = VanillaBlocks::BUDDING_AMETHYST().toBlockState();
        return state;
    }

    const BlockState &amethystClusterState() {
        static const BlockState state = VanillaBlocks::AMETHYST_CLUSTER().toBlockState();
        return state;
    }

}

const char *GeodeFeature::name() const {
    return "minecraft:overworld_amethyst_geode_feature";
}

bool GeodeFeature::_isAmethystShell(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
        return false;

    const BlockState state = manager.getBlockAt(x, y, z);
    return state == amethystBlockState() || state == buddingAmethystState();
}

void GeodeFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();

    mRandom.setSeed(level.getSeed() ^ ((((int64_t) chunk.getX()) << 8) ^ (int64_t) chunk.getZ()));

    BlockManager object(level);
    if (mRandom.nextBoundedInt(24) != 0)
        return;

    const int32_t centerX = mRandom.nextBoundedInt(16);
    const int32_t centerZ = mRandom.nextBoundedInt(16);

    const int32_t outerRadius = 4 + mRandom.nextBoundedInt(5);
    const int32_t calciteThickness = 1;
    const int32_t amethystThickness = 1;
    const double outerRadiusSq = (double) (outerRadius * outerRadius);
    const bool hasCrack = mRandom.nextBoundedInt(100) < 95;
    const double crackAngle = mRandom.nextBoundedInt(360) * MathConstants::PI / 180.0;
    const double crackDirX = std::cos(crackAngle);
    const double crackDirZ = std::sin(crackAngle);
    const double crackHalfWidth = 1.5;
    const double crackHalfHeight = 2.0;

    const int32_t geodeMinY = std::max(LevelChunk::MIN_Y, -58);
    const int32_t geodeMaxY = std::min(LevelChunk::MAX_Y, 30);
    const int32_t minCenterY = geodeMinY + outerRadius;
    const int32_t maxCenterY = geodeMaxY - outerRadius;
    if (minCenterY > maxCenterY)
        return;

    const int32_t centerY = minCenterY + mRandom.nextBoundedInt(maxCenterY - minCenterY + 1);

    const int32_t minX = centerX - outerRadius;
    const int32_t maxX = centerX + outerRadius;
    const int32_t minZ = centerZ - outerRadius;
    const int32_t maxZ = centerZ + outerRadius;
    const int32_t minY = centerY - outerRadius;
    const int32_t maxY = centerY + outerRadius;

    for (int32_t lx = minX; lx <= maxX; lx++) {
        for (int32_t lz = minZ; lz <= maxZ; lz++) {
            for (int32_t y = minY; y <= maxY; y++) {
                const int32_t worldX = (chunk.getX() << 4) + lx;
                const int32_t worldZ = (chunk.getZ() << 4) + lz;

                const double dx = (double) (lx - centerX) + 0.5;
                const double dz = (double) (lz - centerZ) + 0.5;
                const double dy = (double) (y - centerY) + 0.5;
                const double distSq = dx * dx + dy * dy + dz * dz;

                if (distSq > outerRadiusSq)
                    continue;

                if (hasCrack) {
                    const double along = dx * crackDirX + dz * crackDirZ;
                    const double side = std::fabs(dx * -crackDirZ + dz * crackDirX);
                    if (along > 0.0 && along <= (double) outerRadius + 1.0 && side <= crackHalfWidth
                        && std::fabs(dy) <= crackHalfHeight) {
                        object.setBlockStateAt(worldX, y, worldZ, DecorationSupport::airState());
                        continue;
                    }
                }

                const double dist = std::sqrt(distSq);
                const double distFromOuter = (double) outerRadius - dist;

                if (distFromOuter <= 0.5) {
                    object.setBlockStateAt(worldX, y, worldZ, smoothBasaltState());
                } else if (distFromOuter <= 0.5 + (double) calciteThickness) {
                    object.setBlockStateAt(worldX, y, worldZ, calciteState());
                } else if (distFromOuter <= 0.5 + (double) calciteThickness + (double) amethystThickness) {
                    object.setBlockStateAt(worldX, y, worldZ, amethystBlockState());
                    if (mRandom.nextBoundedInt(1000) < 83)
                        object.setBlockStateAt(worldX, y, worldZ, buddingAmethystState());
                } else {
                    object.setBlockStateAt(worldX, y, worldZ, DecorationSupport::airState());

                    const char *touchesAmethystShell = nullptr;
                    if (_isAmethystShell(object, worldX + 1, y, worldZ))
                        touchesAmethystShell = "west";
                    if (_isAmethystShell(object, worldX - 1, y, worldZ))
                        touchesAmethystShell = "east";
                    if (_isAmethystShell(object, worldX, y + 1, worldZ))
                        touchesAmethystShell = "down";
                    if (_isAmethystShell(object, worldX, y - 1, worldZ))
                        touchesAmethystShell = "up";
                    if (_isAmethystShell(object, worldX, y, worldZ + 1))
                        touchesAmethystShell = "north";
                    if (_isAmethystShell(object, worldX, y, worldZ - 1))
                        touchesAmethystShell = "south";

                    if (touchesAmethystShell != nullptr && mRandom.nextBoundedInt(100) < 18)
                        object.setBlockStateAt(worldX, y, worldZ,
                                               DecorationSupport::withState(amethystClusterState(),
                                                                            "minecraft:block_face",
                                                                            touchesAmethystShell));
                }
            }
        }
    }

    queueObject(object);
}
