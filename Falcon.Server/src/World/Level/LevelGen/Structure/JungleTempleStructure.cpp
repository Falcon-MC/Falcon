#include "World/Level/LevelGen/Structure/JungleTempleStructure.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"
#include "World/Level/LevelGen/Structure/ScatteredPiece.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"

#include <array>
#include <random>
#include <utility>

namespace {
    const int32_t WIDTH = 12;
    const int32_t HEIGHT = 10;
    const int32_t DEPTH = 15;
    // the vine_direction_bits the game keeps at 0x1529B2B68: all four sides
    const int32_t VINE_SIDES = 15;
    const float COBBLESTONE_CHANCE = 0.4f;
    const char *const CHEST_LOOT = "loot_tables/chests/jungle_temple.json";
    const char *const DISPENSER_LOOT = "loot_tables/chests/dispenser_trap.json";

    struct Blocks {
        BlockState mCobblestone = VanillaBlocks::COBBLESTONE().toBlockState();
        BlockState mMossy = VanillaBlocks::MOSSY_COBBLESTONE().toBlockState();
        BlockState mAir = VanillaBlocks::AIR().toBlockState();
        BlockState mStairs = VanillaBlocks::STONE_STAIRS().toBlockState();
        BlockState mRedstone = VanillaBlocks::REDSTONE_WIRE().toBlockState();
    };

    // StructurePiece::generateBox with the moss stone selector (sub_147786600, sub_14738C300): one nextFloat for
    // every block of the box, in or out of the chunk
    void mossyBox(ScatteredPiece &piece, const Blocks &blocks, std::mt19937 &random, int32_t minX, int32_t minY,
                  int32_t minZ, int32_t maxX, int32_t maxY, int32_t maxZ) {
        for (int32_t y = minY; y <= maxY; y++) {
            for (int32_t x = minX; x <= maxX; x++) {
                for (int32_t z = minZ; z <= maxZ; z++) {
                    const bool cobblestone = FeatureRandom::nextFloat(random) < COBBLESTONE_CHANCE;
                    piece.placeBlock(cobblestone ? blocks.mCobblestone : blocks.mMossy, x, y, z);
                }
            }
        }
    }

    void airBox(ScatteredPiece &piece, const Blocks &blocks, int32_t minX, int32_t minY, int32_t minZ, int32_t maxX,
                int32_t maxY, int32_t maxZ) {
        piece.generateBox(minX, minY, minZ, maxX, maxY, maxZ, blocks.mAir, blocks.mAir, false);
    }

    BlockState hook(int32_t direction, bool attached) {
        const BlockState base = DecorationSupport::withState(VanillaBlocks::TRIPWIRE_HOOK().toBlockState(),
                                                             "attached_bit", attached ? 1 : 0);
        return DecorationSupport::withState(base, "direction", direction);
    }

    BlockState vine() {
        return DecorationSupport::withState(VanillaBlocks::VINE().toBlockState(), "vine_direction_bits", VINE_SIDES);
    }

    void placeShell(ScatteredPiece &piece, const Blocks &blocks, std::mt19937 &random) {
        mossyBox(piece, blocks, random, 0, -4, 0, WIDTH - 1, 0, DEPTH - 1);
        mossyBox(piece, blocks, random, 2, 1, 2, 9, 2, 2);
        mossyBox(piece, blocks, random, 2, 1, 12, 9, 2, 12);
        mossyBox(piece, blocks, random, 2, 1, 3, 2, 2, 11);
        mossyBox(piece, blocks, random, 9, 1, 3, 9, 2, 11);
        mossyBox(piece, blocks, random, 1, 3, 1, 10, 6, 1);
        mossyBox(piece, blocks, random, 1, 3, 13, 10, 6, 13);
        mossyBox(piece, blocks, random, 1, 3, 2, 1, 6, 12);
        mossyBox(piece, blocks, random, 10, 3, 2, 10, 6, 12);
        mossyBox(piece, blocks, random, 2, 3, 2, 9, 3, 12);
        mossyBox(piece, blocks, random, 2, 6, 2, 9, 6, 12);
        mossyBox(piece, blocks, random, 3, 7, 3, 8, 7, 11);
        mossyBox(piece, blocks, random, 4, 8, 4, 7, 8, 10);

        airBox(piece, blocks, 3, 1, 3, 8, 2, 11);
        airBox(piece, blocks, 4, 3, 6, 7, 3, 9);
        airBox(piece, blocks, 2, 4, 2, 9, 5, 12);
        airBox(piece, blocks, 4, 6, 5, 7, 6, 9);
        airBox(piece, blocks, 5, 7, 6, 6, 7, 8);
        airBox(piece, blocks, 5, 1, 2, 6, 2, 2);
        airBox(piece, blocks, 5, 2, 12, 6, 2, 12);
        airBox(piece, blocks, 5, 5, 1, 6, 5, 1);
        airBox(piece, blocks, 5, 5, 13, 6, 5, 13);
        piece.placeBlock(blocks.mAir, 1, 5, 5);
        piece.placeBlock(blocks.mAir, 10, 5, 5);
        piece.placeBlock(blocks.mAir, 1, 5, 9);
        piece.placeBlock(blocks.mAir, 10, 5, 9);

        // the window columns and the roof posts
        const std::array<std::array<int32_t, 6>, 34> pillars = {
                {{2, 4, 0, 2, 5, 0},   {4, 4, 0, 4, 5, 0},     {7, 4, 0, 7, 5, 0},     {9, 4, 0, 9, 5, 0},
                 {2, 4, 14, 2, 5, 14}, {4, 4, 14, 4, 5, 14},   {7, 4, 14, 7, 5, 14},   {9, 4, 14, 9, 5, 14},
                 {5, 6, 0, 6, 6, 0},   {0, 4, 2, 0, 5, 2},     {0, 4, 4, 0, 5, 4},     {0, 4, 6, 0, 5, 6},
                 {0, 4, 8, 0, 5, 8},   {0, 4, 10, 0, 5, 10},   {0, 4, 12, 0, 5, 12},   {0, 6, 5, 0, 6, 5},
                 {0, 6, 9, 0, 6, 9},   {11, 4, 2, 11, 5, 2},   {11, 4, 4, 11, 5, 4},   {11, 4, 6, 11, 5, 6},
                 {11, 4, 8, 11, 5, 8}, {11, 4, 10, 11, 5, 10}, {11, 4, 12, 11, 5, 12}, {11, 6, 5, 11, 6, 5},
                 {11, 6, 9, 11, 6, 9}, {2, 7, 2, 2, 9, 2},     {9, 7, 2, 9, 9, 2},     {2, 7, 12, 2, 9, 12},
                 {9, 7, 12, 9, 9, 12}, {4, 9, 4, 4, 9, 4},     {7, 9, 4, 7, 9, 4},     {4, 9, 10, 4, 9, 10},
                 {7, 9, 10, 7, 9, 10}, {5, 9, 7, 6, 9, 7}}};
        for (const std::array<int32_t, 6> &box: pillars)
            mossyBox(piece, blocks, random, box[0], box[1], box[2], box[3], box[4], box[5]);
    }

    void placeStairs(ScatteredPiece &piece, const Blocks &blocks, std::mt19937 &random) {
        const BlockState three = ScatteredPiece::stairs(blocks.mStairs, 3);
        const BlockState two = ScatteredPiece::stairs(blocks.mStairs, 2);
        piece.placeBlock(three, 5, 9, 6);
        piece.placeBlock(three, 6, 9, 6);
        piece.placeBlock(two, 5, 9, 8);
        piece.placeBlock(two, 6, 9, 8);
        for (int32_t x = 4; x <= 7; x++)
            piece.placeBlock(three, x, 0, 0);
        for (int32_t step = 0; step < 3; step++)
            piece.placeBlock(three, 4, 1 + step, 8 + step);
        for (int32_t step = 0; step < 3; step++)
            piece.placeBlock(three, 7, 1 + step, 8 + step);

        mossyBox(piece, blocks, random, 4, 1, 9, 4, 1, 9);
        mossyBox(piece, blocks, random, 7, 1, 9, 7, 1, 9);
        mossyBox(piece, blocks, random, 4, 1, 10, 7, 2, 10);
        mossyBox(piece, blocks, random, 5, 4, 5, 6, 4, 5);
        piece.placeBlock(ScatteredPiece::stairs(blocks.mStairs, 0), 4, 4, 5);
        piece.placeBlock(ScatteredPiece::stairs(blocks.mStairs, 1), 7, 4, 5);

        // the stairway down to the lower floor
        for (int32_t step = 0; step < 4; step++) {
            piece.placeBlock(two, 5, -step, 6 + step);
            piece.placeBlock(two, 6, -step, 6 + step);
            for (int32_t x = 5; x <= 6; x++) {
                for (int32_t z = 7; z <= 9; z++)
                    piece.placeBlock(blocks.mAir, x, -step, z + step);
            }
        }
    }

    void placeLowerFloor(ScatteredPiece &piece, const Blocks &blocks, std::mt19937 &random) {
        for (int32_t y = -3; y <= -1; y++) {
            for (int32_t x = 1; x <= 10; x++) {
                piece.placeBlock(blocks.mAir, x, y, 12);
                piece.placeBlock(blocks.mAir, x, y, 13);
            }
        }
        airBox(piece, blocks, 1, -3, 1, 3, -1, 13);
        airBox(piece, blocks, 1, -3, 1, 9, -1, 5);

        for (int32_t z = 1; z <= 13; z += 2)
            mossyBox(piece, blocks, random, 1, -3, z, 1, -2, z);
        for (int32_t z = 2; z <= 12; z += 2)
            mossyBox(piece, blocks, random, 1, -1, z, 3, -1, z);
        mossyBox(piece, blocks, random, 2, -2, 1, 5, -2, 1);
        mossyBox(piece, blocks, random, 7, -2, 1, 9, -2, 1);
        mossyBox(piece, blocks, random, 6, -3, 1, 6, -3, 1);
        mossyBox(piece, blocks, random, 6, -1, 1, 6, -1, 1);
    }

    void placeTraps(ScatteredPiece &piece, const Blocks &blocks, std::mt19937 &random) {
        const BlockState wire = DecorationSupport::withState(VanillaBlocks::TRIP_WIRE().toBlockState(), "attached_bit",
                                                             1);

        // the west trap: hooks and wire across the corridor, wire to the dispenser
        piece.placeBlock(hook(3, true), 1, -3, 8);
        piece.placeBlock(hook(1, true), 4, -3, 8);
        piece.placeBlock(wire, 2, -3, 8);
        piece.placeBlock(wire, 3, -3, 8);
        for (int32_t z = 7; z >= 1; z--)
            piece.placeBlock(blocks.mRedstone, 5, -3, z);
        piece.placeBlock(blocks.mRedstone, 4, -3, 1);
        piece.placeBlock(blocks.mMossy, 3, -3, 1);
        piece.createDispenser(random, 3, -2, 1, 3, DISPENSER_LOOT);
        piece.placeBlock(vine(), 3, -2, 2);

        // the east trap; unlike the west one its hooks stay unattached (0x1473D9925)
        piece.placeBlock(hook(0, false), 7, -3, 1);
        piece.placeBlock(hook(2, false), 7, -3, 5);
        for (int32_t z = 2; z <= 4; z++)
            piece.placeBlock(wire, 7, -3, z);
        piece.placeBlock(blocks.mRedstone, 8, -3, 6);
        piece.placeBlock(blocks.mRedstone, 9, -3, 6);
        piece.placeBlock(blocks.mRedstone, 9, -3, 5);
        piece.placeBlock(blocks.mMossy, 9, -3, 4);
        piece.placeBlock(blocks.mRedstone, 9, -2, 4);
        piece.createDispenser(random, 9, -2, 3, 4, DISPENSER_LOOT);
        piece.placeBlock(vine(), 8, -1, 3);
        piece.placeBlock(vine(), 8, -2, 3);
        piece.createChest(random, 8, -3, 3, 4, CHEST_LOOT);

        const std::array<std::array<int32_t, 3>, 9> mossy = {{{9, -3, 2},
                                                              {8, -3, 1},
                                                              {4, -3, 5},
                                                              {5, -2, 5},
                                                              {5, -1, 5},
                                                              {6, -3, 5},
                                                              {7, -2, 5},
                                                              {7, -1, 5},
                                                              {8, -3, 5}}};
        for (const std::array<int32_t, 3> &block: mossy)
            piece.placeBlock(blocks.mMossy, block[0], block[1], block[2]);
        mossyBox(piece, blocks, random, 9, -1, 1, 9, -1, 5);
    }

    // the hidden room behind the three levers and its piston door
    void placeLeverRoom(ScatteredPiece &piece, const Blocks &blocks, std::mt19937 &random) {
        airBox(piece, blocks, 8, -3, 8, 10, -1, 10);
        const BlockState chiseled = VanillaBlocks::CHISELED_STONE_BRICKS().toBlockState();
        // lever_direction 3: the fourth name of the game's table at 0x14F3B3F48
        const BlockState lever = DecorationSupport::withState(VanillaBlocks::LEVER().toBlockState(), "lever_direction",
                                                              "south");
        for (int32_t x = 8; x <= 10; x++)
            piece.placeBlock(chiseled, x, -2, 11);
        for (int32_t x = 8; x <= 10; x++)
            piece.placeBlock(lever, x, -2, 12);

        mossyBox(piece, blocks, random, 8, -3, 8, 8, -3, 10);
        mossyBox(piece, blocks, random, 10, -3, 8, 10, -3, 10);
        piece.placeBlock(blocks.mMossy, 10, -2, 9);
        piece.placeBlock(blocks.mRedstone, 8, -2, 9);
        piece.placeBlock(blocks.mRedstone, 8, -2, 10);
        piece.placeBlock(blocks.mRedstone, 10, -1, 9);

        const BlockState piston = VanillaBlocks::STICKY_PISTON().toBlockState();
        piece.placeBlock(DecorationSupport::withState(piston, "facing_direction", 1), 9, -2, 8);
        piece.placeBlock(DecorationSupport::withState(piston, "facing_direction", 5), 10, -2, 8);
        piece.placeBlock(DecorationSupport::withState(piston, "facing_direction", 5), 10, -1, 8);
        // direction 0: the repeater's default cardinal_direction, south
        piece.placeBlock(VanillaBlocks::UNPOWERED_REPEATER().toBlockState(), 10, -2, 10);
        piece.createChest(random, 9, -3, 10, 3, CHEST_LOOT);
    }
}

// JunglePyramidPiece::postProcess (sub_1473D4A60, 26.60)
void JungleTempleStructure::place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk) {
    const StructureFeature *temple = StructureFeature::get("temple");
    if (temple == nullptr || !temple->isStartChunk(generator, chunk.getX(), chunk.getZ()))
        return;

    const int32_t cornerX = chunk.getX() * 16;
    const int32_t cornerZ = chunk.getZ() * 16;
    const int32_t surface = generator.getPreliminarySurfaceLevel(chunk.getX() * 4, chunk.getZ() * 4);
    const int32_t biome = generator.pickBiome(cornerX + 8, surface, cornerZ + 8);
    if (biome != BiomeIds::JUNGLE && biome != BiomeIds::JUNGLE_HILLS)
        return;

    const int32_t floorY = ScatteredPiece::averageGround(chunk, 0, 0, WIDTH, DEPTH);
    ScatteredPiece piece(level, chunk, cornerX, floorY, cornerZ);
    std::mt19937 random = StructureFeature::createPostProcessRandom(generator.getSeed(), chunk.getX(), chunk.getZ());
    const Blocks blocks;
    placeShell(piece, blocks, random);
    placeStairs(piece, blocks, random);
    placeLowerFloor(piece, blocks, random);
    placeTraps(piece, blocks, random);
    placeLeverRoom(piece, blocks, random);
    ScatteredPiece::addBounds(level, chunk, "minecraft:jungle_pyramid", 0, cornerX, floorY, cornerZ, WIDTH, HEIGHT,
                              DEPTH);
}
