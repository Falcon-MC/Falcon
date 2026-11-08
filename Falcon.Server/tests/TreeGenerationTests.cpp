#include "Core/Debug/BedrockLog.h"
#include "Core/Debug/ContentLogEndPoint.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/OverworldTreePlacer.h"

#include <functional>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>

namespace {
    using Tree = OverworldTreePlacer;

    const int32_t GROUND = 64;

    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // roofed_tree_feature and roofed_tree_with_vines_feature of the 1.26.61 behavior packs
    const Tree::Acacia ROOFED = {2, 6, {3, 2}, 3, {0, 4}, {0, 3}, {0, 0}, false, {2, 5}, {1, 1}, 1, 3, 2, true,
                                 0, 0, 4, 2, 2, 3};
    const Tree::Definition ROOFED_TREE = {"minecraft:roofed_tree_feature", Tree::Trunk::Acacia,
                                          &VanillaBlocks::DARK_OAK_LOG, {0, 0}, {0, 0}, Tree::Canopy::Roofed,
                                          &VanillaBlocks::DARK_OAK_LEAVES, {0, 0}, {0, 0}, {0, 0}, 0, 0.0f, false};
    const Tree::Definition ROOFED_WITH_VINES = {"minecraft:roofed_tree_with_vines_feature", Tree::Trunk::Acacia,
                                                &VanillaBlocks::DARK_OAK_LOG, {0, 0}, {0, 0}, Tree::Canopy::Roofed,
                                                &VanillaBlocks::DARK_OAK_LEAVES, {0, 0}, {0, 0}, {0, 0}, 0, 0.0f,
                                                false, 0, 1, 0, 0, 0, false, true};

    // grass on dirt over the 3x3 chunks around the origin chunk
    std::unique_ptr<Level> flatLevel() {
        std::unique_ptr<Level> level(new Level("TreeGeneration", 1, 42, DimensionType::Overworld));
        const BlockState dirt = VanillaBlocks::DIRT().toBlockState();
        const BlockState grass = VanillaBlocks::GRASS_BLOCK().toBlockState();
        for (int32_t chunkX = -1; chunkX <= 1; chunkX++) {
            for (int32_t chunkZ = -1; chunkZ <= 1; chunkZ++) {
                LevelChunk chunk(chunkX, chunkZ);
                for (int32_t x = 0; x < 16; x++) {
                    for (int32_t z = 0; z < 16; z++) {
                        for (int32_t y = GROUND - 3; y < GROUND; y++)
                            chunk.setBlock(x, y, z, dirt);
                        chunk.setBlock(x, GROUND, z, grass);
                    }
                }
                level->insertChunk(std::move(chunk));
            }
        }
        return level;
    }

    bool is(BlockManager &blocks, int32_t x, int32_t y, int32_t z, Block (*block)()) {
        return blocks.getBlockAt(x, y, z).mName == block().getIdentifier();
    }

    int32_t trunkHeight(BlockManager &blocks, int32_t x, int32_t z) {
        int32_t height = 0;
        while (is(blocks, x, GROUND + 1 + height, z, &VanillaBlocks::DARK_OAK_LOG))
            height++;
        return height;
    }

    void roofedTrunk() {
        for (uint32_t seed = 1; seed <= 40; seed++) {
            std::unique_ptr<Level> level = flatLevel();
            BlockManager blocks(*level);
            std::mt19937 random(seed);
            require(Tree::place(blocks, ROOFED_TREE, nullptr, nullptr, &ROOFED, 4, GROUND + 1, 4, random, {}),
                    "the roofed tree grows on grass");

            const int32_t height = trunkHeight(blocks, 4, 4);
            require(height >= 6 && height <= 9, "the trunk is 6 plus up to 2 plus up to 1 tall");
            for (int32_t offsetX = 0; offsetX < 2; offsetX++) {
                for (int32_t offsetZ = 0; offsetZ < 2; offsetZ++) {
                    require(trunkHeight(blocks, 4 + offsetX, 4 + offsetZ) == height,
                            "the trunk is a straight 2x2 when diagonal growth is off");
                    require(is(blocks, 4 + offsetX, GROUND, 4 + offsetZ, &VanillaBlocks::DIRT),
                            "the grass under the trunk turns to dirt");
                }
            }

            // every other log hangs from a branch beside the base, below the canopy
            const int32_t top = GROUND + height;
            for (int32_t x = -2; x <= 8; x++) {
                for (int32_t z = -2; z <= 8; z++) {
                    for (int32_t y = GROUND + 1; y <= top + 3; y++) {
                        if (!is(blocks, x, y, z, &VanillaBlocks::DARK_OAK_LOG) || (x >= 4 && x <= 5 && z >= 4 && z <= 5))
                            continue;
                        require(x >= 3 && x <= 6 && z >= 3 && z <= 6, "branches hang on the ring around the trunk");
                        require(y < top && y >= top - 5, "branches hang 1 to 5 under the top log");
                    }
                }
            }
        }
    }

    void roofedCanopy() {
        for (uint32_t seed = 1; seed <= 40; seed++) {
            std::unique_ptr<Level> level = flatLevel();
            BlockManager blocks(*level);
            std::mt19937 random(seed);
            Tree::place(blocks, ROOFED_TREE, nullptr, nullptr, &ROOFED, 4, GROUND + 1, 4, random, {});

            // the canopy is centred on the last log, the corner of the top layer farthest from the base
            const int32_t topY = GROUND + trunkHeight(blocks, 4, 4);
            const int32_t topX = 5;
            const int32_t topZ = 5;
            const auto leaf = [&](int32_t x, int32_t y, int32_t z) {
                return is(blocks, topX + x, topY + y, topZ + z, &VanillaBlocks::DARK_OAK_LEAVES);
            };
            for (int32_t x = -2; x <= 3; x++) {
                for (int32_t z = -2; z <= 3; z++) {
                    if ((x == -1 || x == 0) && (z == -1 || z == 0))
                        continue;
                    require(leaf(x, -1, z) || is(blocks, topX + x, topY - 1, topZ + z, &VanillaBlocks::DARK_OAK_LOG),
                            "the ring under the layer is a full 6x6, branches aside");
                }
            }
            require(!leaf(-2, 1, -2) && !leaf(3, 1, 3) && !leaf(-1, 1, -2), "the ring over it drops three per corner");
            require(leaf(0, 1, -2) && leaf(-2, 1, 0), "the ring over it keeps its sides");
            require(leaf(4, 0, 0) && leaf(-3, 0, 0) && leaf(0, 0, 4) && leaf(0, 0, -3),
                    "the layer reaches 3 before and 4 after the top log");
            require(!leaf(-4, 0, 0) && !leaf(5, 0, 0) && !leaf(-3, 0, -3) && !leaf(-3, 0, 4),
                    "the layer stops at the inner radius and drops its corners");
            const bool capped = leaf(0, 2, 0);
            require(leaf(1, 2, 0) == capped && leaf(0, 2, 1) == capped && leaf(1, 2, 1) == capped && !leaf(-1, 2, 0),
                    "the cap is a whole 2x2 from the top log, or nothing");
        }
    }

    void vinesNeedRoom() {
        std::unique_ptr<Level> level = flatLevel();
        BlockManager blocks(*level);
        blocks.setBlockStateAt(4, GROUND + 1, 4, VanillaBlocks::LEAF_LITTER().toBlockState());

        // with vines the tree only replaces air and leaves, so its room check fails on leaf litter
        std::mt19937 vinesRandom(7);
        require(!Tree::place(blocks, ROOFED_WITH_VINES, nullptr, nullptr, &ROOFED, 4, GROUND + 1, 4, vinesRandom, {}),
                "leaf litter blocks the roofed tree with vines");
        std::mt19937 random(7);
        require(Tree::place(blocks, ROOFED_TREE, nullptr, nullptr, &ROOFED, 4, GROUND + 1, 4, random, {}),
                "the plain roofed tree replaces leaf litter");
        require(is(blocks, 4, GROUND + 1, 4, &VanillaBlocks::DARK_OAK_LOG), "the litter gives way to the trunk");
    }
}

int main() {
    BedrockLog::addEndPoint(std::make_shared<ContentLogEndPoint>(false));
    BlockPaletteRegistry::getInstance().initialize();

    int failures = 0;
    const auto check = [&failures](const char *name, const std::function<void()> &run) {
        try {
            run();
            LOG_INFO(LogAreaID::Server, "PASS: %s", name);
        } catch (const std::exception &error) {
            ++failures;
            LOG_ERROR(LogAreaID::Server, "FAIL: %s: %s", name, error.what());
        }
    };

    check("roofed tree trunk", roofedTrunk);
    check("roofed tree canopy", roofedCanopy);
    check("roofed tree with vines needs room", vinesNeedRoom);

    BedrockLog::flush();
    return failures == 0 ? 0 : 1;
}
