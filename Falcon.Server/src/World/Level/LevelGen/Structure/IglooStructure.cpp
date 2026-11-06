#include "World/Level/LevelGen/Structure/IglooStructure.h"

#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Structure/StructureBlockTable.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"
#include "World/Level/LevelGen/Structure/StructureTemplate.h"

#include <map>
#include <memory>
#include <string>
#include <tuple>

namespace {
    // the shaft is built from three block tall pieces; six is the only length seen in saved worlds so far
    const int32_t SHAFT_PIECES = 6;
    const int32_t SHAFT_PIECE_HEIGHT = 3;
    const int32_t SHAFT_X = 2;
    const int32_t SHAFT_Z = 4;
    const int32_t BOTTOM_Z = -2;
    const int32_t BOTTOM_Y = -24;

    struct IglooChoice {
        int32_t mQuarterTurns = 0;
        bool mHasBasement = false;
    };

    /**
     * Not the game's draw, which is not known yet: this only makes the choice stable per chunk, so igloos differ from
     * BDS in rotation and basement until the real draw replaces this function.
     */
    IglooChoice placeholderChoice(int64_t seed, int32_t chunkX, int32_t chunkZ) {
        uint64_t mixed = (uint64_t) seed ^ ((uint64_t) (uint32_t) chunkX * 0x9E3779B97F4A7C15ULL)
                         ^ ((uint64_t) (uint32_t) chunkZ * 0xC2B2AE3D27D4EB4FULL);
        mixed ^= mixed >> 29;
        mixed *= 0xBF58476D1CE4E5B9ULL;
        mixed ^= mixed >> 32;

        IglooChoice choice;
        choice.mQuarterTurns = (int32_t) (mixed & 3);
        choice.mHasBasement = ((mixed >> 2) & 1) != 0;
        return choice;
    }

    void rotate(int32_t quarterTurns, int32_t &x, int32_t &z) {
        for (int32_t i = 0; i < quarterTurns; ++i) {
            const int32_t oldX = x;
            x = -z;
            z = oldX;
        }
    }

    void addPiece(std::vector<IglooStructure::PlacedBlock> &blocks, const std::string &path, int32_t offsetX,
                  int32_t offsetY, int32_t offsetZ, int32_t quarterTurns) {
        const std::shared_ptr<const StructureTemplate> piece = StructureTemplate::load(path);
        if (piece == nullptr)
            return;

        for (const StructureTemplate::BlockEntry &entry : piece->getBlocks()) {
            IglooStructure::PlacedBlock block;
            if (!StructureBlockTable::translate(piece->getPalette()[(size_t) entry.mPaletteIndex], quarterTurns,
                                                block.mState))
                continue;

            block.x = entry.x + offsetX;
            block.y = entry.y + offsetY;
            block.z = entry.z + offsetZ;
            rotate(quarterTurns, block.x, block.z);
            blocks.push_back(block);
        }
    }

    // the files save bars unconnected and the game connects them once placed
    void connectBars(std::vector<IglooStructure::PlacedBlock> &blocks) {
        std::map<std::tuple<int32_t, int32_t, int32_t>, bool> solid;
        for (const IglooStructure::PlacedBlock &block : blocks)
            solid[{block.x, block.y, block.z}] = block.mState.mName != "minecraft:air";

        const auto isSolid = [&solid](int32_t x, int32_t y, int32_t z) {
            const auto found = solid.find({x, y, z});
            return found != solid.end() && found->second;
        };

        for (IglooStructure::PlacedBlock &block : blocks) {
            if (block.mState.mName != "minecraft:iron_bars")
                continue;

            Tag states = Tag::ofCompound();
            states.putByte("minecraft:connection_east", isSolid(block.x + 1, block.y, block.z) ? 1 : 0);
            states.putByte("minecraft:connection_north", isSolid(block.x, block.y, block.z - 1) ? 1 : 0);
            states.putByte("minecraft:connection_south", isSolid(block.x, block.y, block.z + 1) ? 1 : 0);
            states.putByte("minecraft:connection_west", isSolid(block.x - 1, block.y, block.z) ? 1 : 0);
            block.mState = BlockState(block.mState.mName, states);
        }
    }

    bool isIglooBiome(int32_t biomeId) {
        return biomeId == BiomeIds::ICE_PLAINS || biomeId == BiomeIds::COLD_TAIGA;
    }
}

std::vector<IglooStructure::PlacedBlock> IglooStructure::build(int32_t quarterTurns, bool hasBasement) {
    std::vector<PlacedBlock> blocks;
    addPiece(blocks, hasBasement ? "igloo/igloo_top_trapdoor.nbt" : "igloo/igloo_top_no_trapdoor.nbt", 0, 0, 0,
             quarterTurns);
    if (!hasBasement)
        return blocks;

    for (int32_t i = 1; i <= SHAFT_PIECES; ++i)
        addPiece(blocks, "igloo/igloo_middle.nbt", SHAFT_X, -SHAFT_PIECE_HEIGHT * i, SHAFT_Z, quarterTurns);

    addPiece(blocks, "igloo/igloo_bottom.nbt", 0, BOTTOM_Y, BOTTOM_Z, quarterTurns);
    connectBars(blocks);
    return blocks;
}

void IglooStructure::place(const ChunkGenerator &generator, LevelChunk &chunk) {
    const StructureFeature *temple = StructureFeature::get("temple");
    if (temple == nullptr)
        return;

    for (int32_t startX = chunk.getX() - 1; startX <= chunk.getX() + 1; ++startX) {
        for (int32_t startZ = chunk.getZ() - 1; startZ <= chunk.getZ() + 1; ++startZ) {
            if (!temple->isStartChunk(generator, startX, startZ))
                continue;

            const int32_t cornerX = startX * 16;
            const int32_t cornerZ = startZ * 16;
            const int32_t surface = generator.getPreliminaryHeight(startX * 16 + 8, startZ * 16 + 8);
            if (!isIglooBiome(generator.pickBiome(startX * 16 + 8, surface, startZ * 16 + 8)))
                continue;

            const int32_t floorY = generator.getPreliminaryHeight(cornerX, cornerZ) - 1;
            const IglooChoice choice = placeholderChoice(generator.getSeed(), startX, startZ);
            for (const PlacedBlock &block : build(choice.mQuarterTurns, choice.mHasBasement)) {
                const int32_t x = cornerX + block.x;
                const int32_t z = cornerZ + block.z;
                const int32_t y = floorY + block.y;
                if ((x >> 4) != chunk.getX() || (z >> 4) != chunk.getZ() || y < LevelChunk::MIN_Y)
                    continue;

                chunk.setBlock(x & 15, y, z & 15, block.mState);
            }
        }
    }
}
