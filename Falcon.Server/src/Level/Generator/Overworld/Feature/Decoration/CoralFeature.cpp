#include "Level/Generator/Overworld/Feature/Decoration/CoralFeature.h"

#include "Block/Blocks/VanillaBlocks.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"

#include <array>

namespace {

    const std::array<BlockState, 5> &coralBlocks() {
        static const std::array<BlockState, 5> states = {
                VanillaBlocks::TUBE_CORAL_BLOCK().toBlockState(),
                VanillaBlocks::BRAIN_CORAL_BLOCK().toBlockState(),
                VanillaBlocks::BUBBLE_CORAL_BLOCK().toBlockState(),
                VanillaBlocks::FIRE_CORAL_BLOCK().toBlockState(),
                VanillaBlocks::HORN_CORAL_BLOCK().toBlockState()
        };
        return states;
    }

    const std::array<BlockState, 5> &coralFans() {
        static const std::array<BlockState, 5> states = {
                VanillaBlocks::TUBE_CORAL_FAN().toBlockState(),
                VanillaBlocks::BRAIN_CORAL_FAN().toBlockState(),
                VanillaBlocks::BUBBLE_CORAL_FAN().toBlockState(),
                VanillaBlocks::FIRE_CORAL_FAN().toBlockState(),
                VanillaBlocks::HORN_CORAL_FAN().toBlockState()
        };
        return states;
    }

    const std::array<BlockState, 5> &coralWallFans() {
        static const std::array<BlockState, 5> states = {
                VanillaBlocks::TUBE_CORAL_WALL_FAN().toBlockState(),
                VanillaBlocks::BRAIN_CORAL_WALL_FAN().toBlockState(),
                VanillaBlocks::BUBBLE_CORAL_WALL_FAN().toBlockState(),
                VanillaBlocks::FIRE_CORAL_WALL_FAN().toBlockState(),
                VanillaBlocks::HORN_CORAL_WALL_FAN().toBlockState()
        };
        return states;
    }

    const BlockState &seaPickleState() {
        static const BlockState state = VanillaBlocks::SEA_PICKLE().toBlockState();
        return state;
    }

    bool isCoralState(const BlockState &state) {
        return state.mName.find("coral") != std::string::npos;
    }

    bool isSandState(const BlockState &state) {
        return state.mName == "minecraft:sand" || state.mName == "minecraft:red_sand";
    }

    int32_t coralDirectionFor(const DecorationSupport::FaceOffset &face) {
        if (face.mX == -1)
            return 0;

        if (face.mX == 1)
            return 1;

        if (face.mZ == 1)
            return 3;

        return 2;
    }

}

bool CoralFeature::inBounds(int32_t x, int32_t y, int32_t z) {
    return x >= 0 && x <= 15 && z >= 0 && z <= 15 && y >= LevelChunk::MIN_Y && y <= LevelChunk::MAX_Y;
}

bool CoralFeature::canStay(int32_t x, int32_t y, int32_t z, LevelChunk &chunk, Level &level) const {
    (void) level;

    return inBounds(x, y, z)
           && inBounds(x, y - 1, z)
           && DecorationSupport::isWater(chunk.getBlock(x, y, z))
           && isSandState(chunk.getBlock(x, y - 1, z));
}

void CoralFeature::placeBlock(int32_t x, int32_t y, int32_t z, LevelChunk &chunk, Level &level, IRandom &random) {
    (void) level;

    const BlockState coralState = coralBlocks()[(size_t) random.nextInt((int32_t) coralBlocks().size())];
    placeFeature(chunk, random, x, y, z, coralState);
}

bool CoralFeature::placeCoralBlock(LevelChunk &chunk, IRandom &random, int32_t x, int32_t y, int32_t z,
                                   const BlockState &coralState) {
    if (!inBounds(x, y, z) || !inBounds(x, y + 1, z))
        return false;

    const BlockState target = chunk.getBlock(x, y, z);
    if ((!DecorationSupport::isWater(target) && !isCoralState(target))
        || !DecorationSupport::isWater(chunk.getBlock(x, y + 1, z)))
        return false;

    chunk.setBlock(x, y, z, coralState);

    if (random.nextFloat() < 0.25f) {
        const BlockState topCoral = coralFans()[(size_t) random.nextInt((int32_t) coralFans().size())];
        DecorationSupport::setBlockKeepingWater(chunk, x, y + 1, z, topCoral);
    } else if (random.nextFloat() < 0.05f) {
        const BlockState seaPickle = DecorationSupport::withState(seaPickleState(), "cluster_count", random.nextInt(4));
        DecorationSupport::setBlockKeepingWater(chunk, x, y + 1, z, seaPickle);
    }

    for (const DecorationSupport::FaceOffset &face: DecorationSupport::HORIZONTAL_PLANE) {
        if (random.nextFloat() >= 0.2f)
            continue;

        const int32_t nx = x + face.mX;
        const int32_t nz = z + face.mZ;
        if (!inBounds(nx, y, nz) || !DecorationSupport::isWater(chunk.getBlock(nx, y, nz)))
            continue;

        const BlockState base = coralWallFans()[(size_t) random.nextInt((int32_t) coralWallFans().size())];
        const BlockState wallCoral = DecorationSupport::withState(base, "coral_direction", coralDirectionFor(face));
        DecorationSupport::setBlockKeepingWater(chunk, nx, y, nz, wallCoral);
    }

    return true;
}
