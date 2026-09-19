#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"

#include "Block/BlockData.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Systems/LiquidPhysicsSystem.h"
#include "Level/Generator/Biome/BiomeIds.h"
#include "Level/Generator/Feature/BlockManager.h"
#include "Level/Generator/Feature/IFeature.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"

namespace DecorationSupport {

    const FaceOffset FACE_DOWN = {0, -1, 0};
    const FaceOffset FACE_UP = {0, 1, 0};
    const FaceOffset FACE_NORTH = {0, 0, -1};
    const FaceOffset FACE_SOUTH = {0, 0, 1};
    const FaceOffset FACE_WEST = {-1, 0, 0};
    const FaceOffset FACE_EAST = {1, 0, 0};

    const FaceOffset ALL_FACES[6] = {FACE_DOWN, FACE_UP, FACE_NORTH, FACE_SOUTH, FACE_WEST, FACE_EAST};
    const FaceOffset HORIZONTALS[4] = {FACE_SOUTH, FACE_WEST, FACE_NORTH, FACE_EAST};
    const FaceOffset HORIZONTAL_PLANE[4] = {FACE_NORTH, FACE_EAST, FACE_SOUTH, FACE_WEST};

    int64_t chunkHash(int32_t x, int32_t z) {
        return (((int64_t) x) << 32) | ((int64_t) (uint32_t) z);
    }

    const BlockState &airState() {
        static const BlockState state = VanillaBlocks::AIR().toBlockState();
        return state;
    }

    const BlockState &waterState() {
        static const BlockState state = VanillaBlocks::WATER().toBlockState();
        return state;
    }

    bool isAir(const BlockState &state) {
        return state.mName == "minecraft:air";
    }

    bool isSolid(const BlockState &state) {
        const BlockData *data = BlockDataTable::find(state.mName.c_str());
        if (data == nullptr)
            return false;

        return data->mSolid;
    }

    bool isTransparent(const BlockState &state) {
        const BlockData *data = BlockDataTable::find(state.mName.c_str());
        if (data == nullptr)
            return true;

        return data->mTransparent;
    }

    bool isFullBlock(const BlockState &state) {
        const BlockData *data = BlockDataTable::find(state.mName.c_str());
        if (data == nullptr)
            return false;

        return data->mSolid && !data->mTransparent;
    }

    bool isLiquid(const BlockState &state) {
        return state.mName == "minecraft:water" || state.mName == "minecraft:flowing_water"
               || state.mName == "minecraft:lava" || state.mName == "minecraft:flowing_lava";
    }

    bool isWater(const BlockState &state) {
        return state.mName == "minecraft:water" || state.mName == "minecraft:flowing_water";
    }

    bool isSand(const BlockState &state) {
        return state.mName == "minecraft:sand" || state.mName == "minecraft:red_sand"
               || state.mName == "minecraft:suspicious_sand";
    }

    bool isGrass(const BlockState &state) {
        return state.mName == "minecraft:grass_block";
    }

    bool canBeReplaced(const BlockState &state) {
        return isAir(state) || isLiquid(state);
    }

    void setBlockKeepingWater(LevelChunk &chunk, int32_t x, int32_t y, int32_t z, const BlockState &state) {
        const bool replacedWater = isWater(chunk.getBlock(x, y, z));
        chunk.setBlock(x, y, z, state);

        if (replacedWater && LiquidPhysicsSystem::getWaterloggingLevel(state) > 0)
            chunk.setBlock(x, y, z, 1, waterState());
    }

    int32_t getBiomeAt(Level &level, int32_t x, int32_t y, int32_t z) {
        LevelChunk *chunk = level.peekChunkPtr(x >> 4, z >> 4);
        if (chunk == nullptr)
            return -1;

        return (int32_t) chunk->getBiomeAt(x & 15, y, z & 15);
    }

    int32_t getHeightAt(Level &level, int32_t x, int32_t z) {
        return level.getHeightAt(x, z);
    }

    bool isChunkGenerated(Level &level, int32_t chunkX, int32_t chunkZ) {
        return level.isChunkResident(chunkX, chunkZ);
    }

    bool isBiomeForest(int32_t biomeId) {
        return biomeId == BiomeIds::FOREST
               || biomeId == BiomeIds::BIRCH_FOREST
               || biomeId == BiomeIds::BIRCH_FOREST_MUTATED
               || biomeId == BiomeIds::FLOWER_FOREST
               || biomeId == BiomeIds::ROOFED_FOREST;
    }

    bool isBiomeBirch(int32_t biomeId) {
        return biomeId == BiomeIds::BIRCH_FOREST || biomeId == BiomeIds::BIRCH_FOREST_MUTATED;
    }

    bool isBiomeStone(int32_t biomeId) {
        return biomeId == BiomeIds::MESA
               || biomeId == BiomeIds::MESA_PLATEAU_STONE
               || biomeId == BiomeIds::MESA_BRYCE
               || biomeId == BiomeIds::STONE_BEACH
               || biomeId == BiomeIds::STONY_PEAKS;
    }

    bool isBiomeTaiga(int32_t biomeId) {
        return biomeId == BiomeIds::TAIGA
               || biomeId == BiomeIds::COLD_TAIGA
               || biomeId == BiomeIds::MEGA_TAIGA
               || biomeId == BiomeIds::REDWOOD_TAIGA_MUTATED;
    }

    bool isBiomeSwamp(int32_t biomeId) {
        return biomeId == BiomeIds::SWAMPLAND || biomeId == BiomeIds::MANGROVE_SWAMP;
    }

    bool isBiomeRoofed(int32_t biomeId) {
        return biomeId == BiomeIds::ROOFED_FOREST;
    }

    bool isBiomeMooshroomIsland(int32_t biomeId) {
        return biomeId == BiomeIds::MUSHROOM_ISLAND;
    }

    bool isBiomePaleGarden(int32_t biomeId) {
        return biomeId == BiomeIds::PALE_GARDEN;
    }

    bool isBiomeCherryGrove(int32_t biomeId) {
        return biomeId == BiomeIds::CHERRY_GROVE;
    }

    bool isBiomeBamboo(int32_t biomeId) {
        return biomeId == BiomeIds::BAMBOO_JUNGLE;
    }

    bool reedsSupportValid(Level &level, const BlockState &support, int32_t x, int32_t y, int32_t z) {
        if (support.mName == "minecraft:reeds")
            return true;

        if (!IFeature::isSupportDirt(support) && !isSand(support))
            return false;

        for (const FaceOffset &face: HORIZONTAL_PLANE) {
            const BlockState *side = level.peekBlockPtr(x + face.mX, y + face.mY, z + face.mZ);
            if (side == nullptr)
                continue;

            if (isWater(*side) || side->mName == "minecraft:frosted_ice")
                return true;
        }

        return false;
    }

    BlockState withState(const BlockState &base, const char *key, int32_t value) {
        Tag states = base.mStates;
        const Tag *existing = states.get(key);
        if (existing != nullptr && existing->getType() == Tag::Type::Byte)
            states.putByte(key, (int8_t) value);
        else
            states.putInt(key, value);

        return BlockState(base.mName, states);
    }

    BlockState withState(const BlockState &base, const char *key, const char *value) {
        Tag states = base.mStates;
        states.putString(key, value);
        return BlockState(base.mName, states);
    }

    BlockState withByteState(const BlockState &base, const char *key, bool value) {
        Tag states = base.mStates;
        states.putByte(key, value ? 1 : 0);
        return BlockState(base.mName, states);
    }

}
