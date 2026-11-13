#include "World/Level/LevelGen/Structure/DesertPyramidStructure.h"

#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/GeneratorChunkSource.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"
#include "World/Level/LevelGen/Random/XoroshiroRandom.h"
#include "World/Level/LevelGen/Structure/ScatteredPiece.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"

#include <algorithm>
#include <array>
#include <random>
#include <tuple>
#include <utility>
#include <vector>

namespace {
    const int32_t WIDTH = 21;
    const int32_t DEPTH = 21;
    const int32_t BURY_RANGE = 3;
    const int32_t MIN_UNCOVERED = 5;
    const char *const LOOT_TABLE = "loot_tables/chests/desert_pyramid.json";
    const char *const START = "desert_pyramid";

    using Position = std::tuple<int32_t, int32_t, int32_t>;

    struct Blocks {
        BlockState mSandstone = VanillaBlocks::SANDSTONE().toBlockState();
        BlockState mCut = VanillaBlocks::CUT_SANDSTONE().toBlockState();
        BlockState mChiseled = VanillaBlocks::CHISELED_SANDSTONE().toBlockState();
        BlockState mOrange = VanillaBlocks::ORANGE_TERRACOTTA().toBlockState();
        BlockState mBlue = VanillaBlocks::BLUE_TERRACOTTA().toBlockState();
        BlockState mAir = VanillaBlocks::AIR().toBlockState();
        BlockState mStairs = VanillaBlocks::SANDSTONE_STAIRS().toBlockState();
        BlockState mSand = VanillaBlocks::SAND().toBlockState();
    };

    // Mth::getSeed with the product cut to 32 bits before the xoroshiro seeding
    int64_t positionSeed(int32_t x, int32_t y, int32_t z) {
        const int64_t mixed = (116129781LL * (int64_t) z)
                              ^ (int64_t) (int32_t) (((uint32_t) x * 3129871U) ^ (uint32_t) y);
        const uint64_t spread = (uint64_t) mixed * (42317861ULL * (uint64_t) mixed + 11ULL);
        return (int64_t) (int32_t) (spread >> 16);
    }

    bool isDesertStart(const ChunkGenerator &generator, int32_t startX, int32_t startZ) {
        const StructureFeature *temple = StructureFeature::get("temple");
        if (temple == nullptr || !temple->isStartChunk(generator, startX, startZ))
            return false;

        const int32_t surface = generator.getPreliminarySurfaceLevel(startX * 4, startZ * 4);
        const int32_t biome = generator.pickBiome(startX * 16 + 8, surface, startZ * 16 + 8);
        return biome == BiomeIds::DESERT || biome == BiomeIds::DESERT_HILLS;
    }

    // the lowest ground of the box columns in this chunk, as the first chunk to post process the piece sees it
    int32_t lowestGround(const LevelChunk &chunk, int32_t minX, int32_t minZ) {
        int32_t lowest = LevelChunk::MAX_Y + 1;
        for (int32_t z = std::max(minZ, chunk.getZ() * 16); z <= std::min(minZ + DEPTH - 1, chunk.getZ() * 16 + 15);
             z++) {
            for (int32_t x = std::max(minX, chunk.getX() * 16); x <= std::min(minX + WIDTH - 1, chunk.getX() * 16 + 15);
                 x++)
                lowest = std::min(lowest, ScatteredPiece::aboveTopSolid(chunk, x & 15, z & 15));
        }
        return lowest;
    }

    // DesertPyramidPiece::postProcess (sub_1473CE9F0, 26.60), first draw: the piece keeps the floor of the first
    // chunk it is post processed in (+0x44), whatever the others see
    int32_t floorY(Level &level, const LevelChunk &chunk, int32_t startX, int32_t startZ, int32_t lowered) {
        const int32_t minX = startX * 16;
        const int32_t minZ = startZ * 16;
        TerrainCache *cache = level.getTerrainCache();
        int32_t floor = 0;
        if (cache != nullptr && cache->findStartValue(START, startX, startZ, floor))
            return floor;

        floor = lowestGround(chunk, minX, minZ) - lowered;
        return cache == nullptr ? floor : cache->storeStartValue(START, startX, startZ, floor);
    }

    void placeTower(ScatteredPiece &piece, const Blocks &blocks, int32_t x, const std::array<BlockState, 4> &stairs) {
        piece.generateBox(x, 0, 0, x + 4, 9, 4, blocks.mSandstone, blocks.mAir, false);
        piece.generateBox(x + 1, 10, 1, x + 3, 10, 3, blocks.mSandstone, blocks.mSandstone, false);
        piece.placeBlock(stairs[3], x + 2, 10, 0);
        piece.placeBlock(stairs[2], x + 2, 10, 4);
        piece.placeBlock(stairs[0], x, 10, 2);
        piece.placeBlock(stairs[1], x + 4, 10, 2);
    }

    // the terracotta pattern of the outer walls, three blocks wide from y 2 to 8
    const BlockState &wallBlock(const Blocks &blocks, int32_t y, int32_t column) {
        const std::array<const BlockState *, 21> pattern = {
                &blocks.mCut,    &blocks.mOrange,   &blocks.mCut,    &blocks.mCut,    &blocks.mOrange, &blocks.mCut,
                &blocks.mOrange, &blocks.mChiseled, &blocks.mOrange, &blocks.mCut,    &blocks.mOrange, &blocks.mCut,
                &blocks.mOrange, &blocks.mChiseled, &blocks.mOrange, &blocks.mOrange, &blocks.mOrange, &blocks.mOrange,
                &blocks.mCut,    &blocks.mCut,      &blocks.mCut};
        return *pattern[(size_t) ((y - 2) * 3 + column)];
    }

    void placeSideWall(ScatteredPiece &piece, const Blocks &blocks, int32_t x) {
        for (int32_t y = 2; y <= 8; y++) {
            for (int32_t z = 1; z <= 3; z++)
                piece.placeBlock(wallBlock(blocks, y, z - 1), x, y, z);
        }
    }

    void placeFrontWall(ScatteredPiece &piece, const Blocks &blocks, int32_t x) {
        for (int32_t y = 2; y <= 8; y++) {
            for (int32_t offset = -1; offset <= 1; offset++)
                piece.placeBlock(wallBlock(blocks, y, offset + 1), x + offset, y, 0);
        }
    }

    // a block actor of the brushable kind stands there: the game draws its loot seed (sub_1473D4780)
    bool isBrushable(Level &level, const ScatteredPiece &piece, int32_t x, int32_t y, int32_t z) {
        const BlockState *state = piece.getBlock(x, y, z);
        if (state == nullptr)
            state = level.peekBlockPtr(piece.worldX(x), piece.worldY(y), piece.worldZ(z));
        return state != nullptr
               && (state->mName == BlockNames::SUSPICIOUS_SAND || state->mName == BlockNames::SUSPICIOUS_GRAVEL);
    }

    // the hidden room under the sand of the east side, worlds from 1.20 on
    void placeCellar(Level &level, ScatteredPiece &piece, const Blocks &blocks, std::mt19937 &random) {
        const bool swap = FeatureRandom::nextBoolean(random);
        const BlockState stairs = ScatteredPiece::stairs(blocks.mStairs, 1);
        piece.placeBlock(stairs, 13, -1, 17);
        piece.placeBlock(stairs, 14, -2, 17);
        piece.placeBlock(stairs, 15, -3, 17);
        for (int32_t x = 12; x <= 16; x++)
            piece.placeBlock(blocks.mSand, x, 0, 17);
        piece.placeBlock(blocks.mSand, 14, -1, 17);
        piece.placeBlock(swap ? blocks.mSand : blocks.mSandstone, 15, -1, 17);
        piece.placeBlock(swap ? blocks.mSandstone : blocks.mSand, 16, -1, 17);
        piece.placeBlock(blocks.mSand, 15, -2, 17);
        piece.placeBlock(blocks.mSandstone, 16, -2, 17);
        piece.placeBlock(blocks.mSand, 16, -3, 17);

        for (int32_t y = -3; y <= -1; y++) {
            piece.generateBox(13, y, 10, 13, y, 15, blocks.mCut, blocks.mCut, true);
            piece.generateBox(19, y, 10, 19, y, 15, blocks.mCut, blocks.mCut, true);
            piece.generateBox(13, y, 10, 19, y, 11, blocks.mCut, blocks.mCut, true);
            piece.generateBox(13, y, 16, 19, y, 16, blocks.mCut, blocks.mCut, true);
        }

        XoroshiroRandom archaeology(positionSeed(piece.worldX(16), piece.worldY(-4), piece.worldZ(13)));
        std::vector<Position> candidates;
        for (int32_t y = -3; y <= -1; y++) {
            for (int32_t x = 14; x <= 18; x++) {
                for (int32_t z = 11; z <= 15; z++) {
                    piece.placeBlock(blocks.mSand, x, y, z);
                    candidates.emplace_back(x, y, z);
                }
            }
        }
        for (int32_t x = 14; x <= 18; x++) {
            for (int32_t z = 11; z <= 15; z++)
                piece.placeBlock(archaeology.nextInt(3) != 0 ? blocks.mSand : blocks.mSandstone, x, 0, z);
        }

        piece.placeBlock(blocks.mBlue, 16, -4, 13);
        const std::array<std::pair<int32_t, int32_t>, 9> orange = {
                {{17, 14}, {17, 12}, {15, 14}, {15, 12}, {18, 13}, {14, 13}, {16, 15}, {16, 11}, {19, 13}}};
        for (const std::pair<int32_t, int32_t> &spot: orange)
            piece.placeBlock(blocks.mOrange, spot.first, -4, spot.second);

        // the four openings: terracotta floor, two sand in the gap, a cut and chiseled face beyond but on the stairs
        // side; the first one's terracotta is the last of the ring
        struct Opening {
            int32_t x;
            int32_t z;
            bool mFloor;
            bool mFace;
            int32_t mFaceX;
            int32_t mFaceZ;
        };
        const std::array<Opening, 4> openings = {{{19, 13, false, true, 20, 13},
                                                  {13, 13, true, true, 12, 13},
                                                  {16, 16, true, false, 0, 0},
                                                  {16, 10, true, true, 16, 9}}};
        for (const Opening &opening: openings) {
            if (opening.mFloor)
                piece.placeBlock(blocks.mOrange, opening.x, -4, opening.z);
            piece.placeBlock(blocks.mSand, opening.x, -3, opening.z);
            candidates.emplace_back(opening.x, -3, opening.z);
            piece.placeBlock(blocks.mSand, opening.x, -2, opening.z);
            candidates.emplace_back(opening.x, -2, opening.z);
            if (!opening.mFace)
                continue;

            piece.placeBlock(blocks.mCut, opening.mFaceX, -3, opening.mFaceZ);
            piece.placeBlock(blocks.mChiseled, opening.mFaceX, -2, opening.mFaceZ);
        }

        const int32_t uncovered = std::min(archaeology.nextInt(BURY_RANGE) + MIN_UNCOVERED,
                                           (int32_t) candidates.size());
        for (size_t i = 1; i < candidates.size(); ++i) {
            const size_t j = (size_t) archaeology.nextInt((int32_t) i + 1);
            if (j != i)
                std::swap(candidates[i], candidates[j]);
        }

        const BlockState suspicious = VanillaBlocks::SUSPICIOUS_SAND().toBlockState();
        const auto uncover = [&](const Position &position) {
            const int32_t x = std::get<0>(position);
            const int32_t y = std::get<1>(position);
            const int32_t z = std::get<2>(position);
            piece.placeBlock(suspicious, x, y, z);
            // the loot seed of the brushable block; no brushable block actor here to keep it
            if (isBrushable(level, piece, x, y, z))
                archaeology.nextLong();
        };
        for (int32_t i = 0; i < uncovered; ++i)
            uncover(candidates[(size_t) i]);

        const int32_t floorX = archaeology.nextInt(5) + 14;
        const int32_t floorZ = archaeology.nextInt(5) + 11;
        uncover(Position(floorX, 0, floorZ));
    }

    void placePyramid(Level &level, ScatteredPiece &piece, std::mt19937 &random) {
        const Blocks blocks;
        piece.generateBox(0, -4, 0, WIDTH - 1, 0, DEPTH - 1, blocks.mSandstone, blocks.mSandstone, false);
        for (int32_t i = 1; i <= 9; i++) {
            piece.generateBox(i, i, i, WIDTH - 1 - i, i, DEPTH - 1 - i, blocks.mSandstone, blocks.mSandstone, false);
            piece.generateBox(i + 1, i, i + 1, WIDTH - 2 - i, i, DEPTH - 2 - i, blocks.mAir, blocks.mAir, false);
        }
        for (int32_t x = 0; x < WIDTH; x++) {
            for (int32_t z = 0; z < DEPTH; z++)
                piece.fillColumnDown(blocks.mSandstone, x, -5, z);
        }

        const std::array<BlockState, 4> stairs = {
                ScatteredPiece::stairs(blocks.mStairs, 0), ScatteredPiece::stairs(blocks.mStairs, 1),
                ScatteredPiece::stairs(blocks.mStairs, 2), ScatteredPiece::stairs(blocks.mStairs, 3)};
        placeTower(piece, blocks, 0, stairs);
        placeTower(piece, blocks, WIDTH - 5, stairs);

        piece.generateBox(8, 0, 0, 12, 4, 4, blocks.mSandstone, blocks.mAir, false);
        piece.generateBox(9, 1, 0, 11, 3, 4, blocks.mAir, blocks.mAir, false);
        const std::array<std::pair<int32_t, int32_t>, 7> door = {
                {{9, 1}, {9, 2}, {9, 3}, {10, 3}, {11, 3}, {11, 2}, {11, 1}}};
        for (const std::pair<int32_t, int32_t> &block: door)
            piece.placeBlock(blocks.mCut, block.first, block.second, 1);

        piece.generateBox(4, 1, 1, 8, 3, 3, blocks.mSandstone, blocks.mAir, false);
        piece.generateBox(4, 1, 2, 8, 2, 2, blocks.mAir, blocks.mAir, false);
        piece.generateBox(12, 1, 1, 16, 3, 3, blocks.mSandstone, blocks.mAir, false);
        piece.generateBox(12, 1, 2, 16, 2, 2, blocks.mAir, blocks.mAir, false);
        piece.generateBox(5, 4, 5, WIDTH - 6, 4, DEPTH - 6, blocks.mSandstone, blocks.mSandstone, false);
        piece.generateBox(9, 4, 9, 11, 4, 11, blocks.mAir, blocks.mAir, false);
        piece.generateBox(8, 1, 8, 8, 3, 8, blocks.mCut, blocks.mSandstone, false);
        piece.generateBox(12, 1, 8, 12, 3, 8, blocks.mCut, blocks.mSandstone, false);
        piece.generateBox(8, 1, 12, 8, 3, 12, blocks.mCut, blocks.mSandstone, false);
        piece.generateBox(12, 1, 12, 12, 3, 12, blocks.mCut, blocks.mSandstone, false);
        piece.generateBox(1, 1, 5, 4, 4, 11, blocks.mSandstone, blocks.mSandstone, false);
        piece.generateBox(WIDTH - 5, 1, 5, WIDTH - 2, 4, 11, blocks.mSandstone, blocks.mSandstone, false);
        piece.generateBox(6, 7, 9, 6, 7, 11, blocks.mSandstone, blocks.mSandstone, false);
        piece.generateBox(WIDTH - 7, 7, 9, WIDTH - 7, 7, 11, blocks.mSandstone, blocks.mSandstone, false);
        piece.generateBox(5, 5, 9, 5, 7, 11, blocks.mCut, blocks.mCut, false);
        piece.generateBox(WIDTH - 6, 5, 9, WIDTH - 6, 7, 11, blocks.mCut, blocks.mCut, false);
        piece.placeBlock(blocks.mAir, 5, 5, 10);
        piece.placeBlock(blocks.mAir, 5, 6, 10);
        piece.placeBlock(blocks.mAir, 6, 6, 10);
        piece.placeBlock(blocks.mAir, WIDTH - 6, 5, 10);
        piece.placeBlock(blocks.mAir, WIDTH - 6, 6, 10);
        piece.placeBlock(blocks.mAir, WIDTH - 7, 6, 10);

        piece.generateBox(2, 4, 4, 2, 6, 4, blocks.mAir, blocks.mAir, false);
        piece.generateBox(WIDTH - 3, 4, 4, WIDTH - 3, 6, 4, blocks.mAir, blocks.mAir, false);
        piece.placeBlock(stairs[3], 2, 4, 5);
        piece.placeBlock(stairs[3], 2, 3, 4);
        piece.placeBlock(stairs[3], WIDTH - 3, 4, 5);
        piece.placeBlock(stairs[3], WIDTH - 3, 3, 4);
        piece.generateBox(1, 1, 3, 2, 2, 3, blocks.mSandstone, blocks.mSandstone, false);
        piece.generateBox(WIDTH - 3, 1, 3, WIDTH - 2, 2, 3, blocks.mSandstone, blocks.mSandstone, false);
        piece.placeBlock(blocks.mStairs, 1, 1, 2);
        piece.placeBlock(blocks.mStairs, WIDTH - 2, 1, 2);
        const BlockState slab = VanillaBlocks::SANDSTONE_SLAB().toBlockState();
        piece.placeBlock(slab, 1, 2, 2);
        piece.placeBlock(slab, WIDTH - 2, 2, 2);
        piece.placeBlock(stairs[1], 2, 1, 2);
        piece.placeBlock(stairs[0], WIDTH - 3, 1, 2);

        // the west ledge runs one block further north than the east one
        piece.generateBox(4, 3, 5, 4, 3, 18, blocks.mSandstone, blocks.mSandstone, false);
        piece.generateBox(WIDTH - 5, 3, 5, WIDTH - 5, 3, 17, blocks.mSandstone, blocks.mSandstone, false);
        piece.generateBox(3, 1, 5, 4, 2, 16, blocks.mAir, blocks.mAir, false);
        piece.generateBox(WIDTH - 6, 1, 5, WIDTH - 5, 2, 16, blocks.mAir, blocks.mAir, false);
        for (int32_t z = 5; z <= 17; z += 2) {
            piece.placeBlock(blocks.mCut, 4, 1, z);
            piece.placeBlock(blocks.mChiseled, 4, 2, z);
            piece.placeBlock(blocks.mCut, WIDTH - 5, 1, z);
            piece.placeBlock(blocks.mChiseled, WIDTH - 5, 2, z);
        }

        const std::array<std::pair<int32_t, int32_t>, 12> floor = {{{10, 7},
                                                                    {10, 8},
                                                                    {9, 9},
                                                                    {11, 9},
                                                                    {8, 10},
                                                                    {12, 10},
                                                                    {7, 10},
                                                                    {13, 10},
                                                                    {9, 11},
                                                                    {11, 11},
                                                                    {10, 12},
                                                                    {10, 13}}};
        for (const std::pair<int32_t, int32_t> &spot: floor)
            piece.placeBlock(blocks.mOrange, spot.first, 0, spot.second);
        piece.placeBlock(blocks.mBlue, 10, 0, 10);

        for (int32_t x = 0; x < WIDTH; x += WIDTH - 1)
            placeSideWall(piece, blocks, x);
        for (int32_t x = 2; x <= WIDTH - 3; x += WIDTH - 5)
            placeFrontWall(piece, blocks, x);

        piece.generateBox(8, 4, 0, 12, 6, 0, blocks.mCut, blocks.mCut, false);
        piece.placeBlock(blocks.mAir, 8, 6, 0);
        piece.placeBlock(blocks.mAir, 12, 6, 0);
        piece.placeBlock(blocks.mOrange, 9, 5, 0);
        piece.placeBlock(blocks.mChiseled, 10, 5, 0);
        piece.placeBlock(blocks.mOrange, 11, 5, 0);

        piece.generateBox(8, -14, 8, 12, -11, 12, blocks.mCut, blocks.mCut, false);
        piece.generateBox(8, -10, 8, 12, -10, 12, blocks.mChiseled, blocks.mChiseled, false);
        piece.generateBox(8, -9, 8, 12, -9, 12, blocks.mCut, blocks.mCut, false);
        piece.generateBox(8, -8, 8, 12, -1, 12, blocks.mSandstone, blocks.mSandstone, false);
        piece.generateBox(9, -11, 9, 11, -1, 11, blocks.mAir, blocks.mAir, false);
        piece.placeBlock(VanillaBlocks::STONE_PRESSURE_PLATE().toBlockState(), 10, -11, 10);
        piece.generateBox(9, -13, 9, 11, -13, 11, VanillaBlocks::TNT().toBlockState(), blocks.mAir, false);

        // the four chest alcoves: two air, chiseled over cut beyond
        const std::array<std::array<int32_t, 4>, 4> alcoves = {
                {{8, 10, 7, 10}, {12, 10, 13, 10}, {10, 8, 10, 7}, {10, 12, 10, 13}}};
        for (const std::array<int32_t, 4> &alcove: alcoves) {
            piece.placeBlock(blocks.mAir, alcove[0], -11, alcove[1]);
            piece.placeBlock(blocks.mAir, alcove[0], -10, alcove[1]);
            piece.placeBlock(blocks.mChiseled, alcove[2], -10, alcove[3]);
            piece.placeBlock(blocks.mCut, alcove[2], -11, alcove[3]);
        }

        piece.createChest(random, 10, -11, 12, 2, LOOT_TABLE);
        piece.createChest(random, 8, -11, 10, 5, LOOT_TABLE);
        piece.createChest(random, 10, -11, 8, 3, LOOT_TABLE);
        piece.createChest(random, 12, -11, 10, 4, LOOT_TABLE);

        placeCellar(level, piece, blocks, random);
    }
}

void DesertPyramidStructure::place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk) {
    // the 21 block piece reaches into the chunks east and south of its start
    for (int32_t startX = chunk.getX() - 1; startX <= chunk.getX(); startX++) {
        for (int32_t startZ = chunk.getZ() - 1; startZ <= chunk.getZ(); startZ++) {
            if (!isDesertStart(generator, startX, startZ))
                continue;

            std::mt19937 random = StructureFeature::createPostProcessRandom(generator.getSeed(), chunk.getX(),
                                                                            chunk.getZ());
            const int32_t lowered = FeatureRandom::nextInt(random, 3);
            ScatteredPiece piece(level, chunk, startX * 16, floorY(level, chunk, startX, startZ, lowered), startZ * 16);
            placePyramid(level, piece, random);
        }
    }
}
