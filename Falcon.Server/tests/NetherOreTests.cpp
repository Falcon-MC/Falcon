#include "Core/Debug/BedrockLog.h"
#include "Core/Debug/ContentLogEndPoint.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/OverworldRulePlacer.h"

#include <functional>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // netherrack over the 3x3 chunks around the origin chunk
    std::unique_ptr<Level> netherrackLevel() {
        std::unique_ptr<Level> level(new Level("NetherOre", 1, 42, DimensionType::Nether));
        const BlockState netherrack = VanillaBlocks::NETHERRACK().toBlockState();
        for (int32_t chunkX = -1; chunkX <= 1; chunkX++) {
            for (int32_t chunkZ = -1; chunkZ <= 1; chunkZ++) {
                LevelChunk chunk(chunkX, chunkZ);
                for (int32_t x = 0; x < 16; x++) {
                    for (int32_t z = 0; z < 16; z++) {
                        for (int32_t y = 0; y < 128; y++)
                            chunk.setBlock(x, y, z, netherrack);
                    }
                }
                level->insertChunk(std::move(chunk));
            }
        }
        return level;
    }

    int32_t count(BlockManager &blocks, Block (*block)()) {
        const std::string name = block().getIdentifier();
        int32_t found = 0;
        for (int32_t x = -16; x < 32; x++) {
            for (int32_t z = -16; z < 32; z++) {
                for (int32_t y = 30; y < 70; y++)
                    found += blocks.getBlockAt(x, y, z).mName == name;
            }
        }
        return found;
    }

    // the nether decoration builds each vein's rule on the stack: every ore must still place its own block
    void eachVeinPlacesItsBlock() {
        const std::unique_ptr<Level> level = netherrackLevel();
        BlockManager blocks(*level);
        const std::vector<std::string> netherrack = {"minecraft:netherrack"};
        Block (*const ores[])() = {&VanillaBlocks::NETHER_GOLD_ORE, &VanillaBlocks::QUARTZ_ORE,
                                   &VanillaBlocks::MAGMA, &VanillaBlocks::GRAVEL, &VanillaBlocks::BLACKSTONE};
        for (Block (*ore)(): ores) {
            const int32_t before = count(blocks, ore);
            std::mt19937 random(7);
            require(OverworldRulePlacer::placeOreVein(blocks, 14, ore, netherrack, -8, 50, -8, random),
                    "a vein placed nothing in netherrack");
            require(count(blocks, ore) > before, "a vein placed another ore's block");
            for (int32_t x = -16; x < 32; x++) {
                for (int32_t z = -16; z < 32; z++) {
                    for (int32_t y = 30; y < 70; y++) {
                        if (blocks.getBlockAt(x, y, z).mName != "minecraft:netherrack")
                            blocks.setBlockStateAt(x, y, z, VanillaBlocks::NETHERRACK().toBlockState());
                    }
                }
            }
        }
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

    check("each nether vein places its own block", eachVeinPlacesItsBlock);
    return failures == 0 ? 0 : 1;
}
