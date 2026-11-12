#include "World/Level/LevelGen/Structure/ShipwreckStructure.h"

#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"
#include "World/Level/LevelGen/Structure/ShipwreckFeature.h"
#include "World/Level/LevelGen/Structure/StructureBlockTable.h"
#include "World/Level/LevelGen/Structure/StructureConnections.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"
#include "World/Level/LevelGen/Structure/StructureMarkers.h"
#include "World/Level/LevelGen/Structure/StructureTerrain.h"
#include "World/Level/LevelGen/Structure/StructureTemplate.h"
#include "World/Level/Block/BlockNames.h"

#include <array>
#include <map>
#include <memory>
#include <tuple>
#include <utility>

namespace {
    const int32_t SEA_LEVEL = 63;
    const int32_t MAX_REACH_CHUNKS = 2;
    const int32_t PIECE_COUNT = 20;
    const int32_t BEACHED_PIECE_COUNT = 14;
    const int32_t WOOD_PAIR_COUNT = 9;
    const int32_t CHEST_KEPT_RANGE = 10;
    const int32_t CHEST_KEPT_FROM = 5;

    const std::array<const char *, PIECE_COUNT> PIECES = {"swwithmast",
                                                          "swwithmastdegraded",
                                                          "swsidewaysfull",
                                                          "swsidewaysfronthalf",
                                                          "swsidewaysbackhalf",
                                                          "swsidewaysfulldegraded",
                                                          "swsidewaysfronthalfdegraded",
                                                          "swsidewaysbackhalfdegraded",
                                                          "swrightsideupfull",
                                                          "swrightsideupfronthalf",
                                                          "swrightsideupbackhalf",
                                                          "swrightsideupfulldegraded",
                                                          "swrightsideupfronthalfdegraded",
                                                          "swrightsideupbackhalfdegraded",
                                                          "swupsidedownfull",
                                                          "swupsidedownfronthalf",
                                                          "swupsidedownbackhalf",
                                                          "swupsidedownfulldegraded",
                                                          "swupsidedownfronthalfdegraded",
                                                          "swupsidedownbackhalfdegraded"};

    const std::array<const char *, 6> SPECIES = {"oak", "spruce", "birch", "jungle", "acacia", "dark_oak"};

    // what the oak and the dark oak of the piece become, by species
    const std::array<std::pair<int32_t, int32_t>, WOOD_PAIR_COUNT> WOOD_PAIRS = {
            {{0, 1}, {3, 1}, {5, 3}, {5, 1}, {1, 0}, {1, 3}, {1, 5}, {0, 2}, {1, 4}}};

    BlockState waterState() {
        Tag states = Tag::ofCompound();
        states.putInt("liquid_depth", 0);
        return BlockState("minecraft:water", states);
    }

    // the mean height of the middle half of the wreck
    int32_t meanHeight(StructureTerrain &terrain, int32_t startX, int32_t startZ, int32_t turns, int32_t sizeX,
                       int32_t sizeZ) {
        const bool flippedX = turns == 1 || turns == 2;
        const bool flippedZ = turns == 2 || turns == 3;
        const int32_t fromX = flippedX ? startX + 15 + (-3 * sizeX) / 4 : startX + sizeX / 4;
        const int32_t toX = flippedX ? startX + 15 + sizeX / -4 : startX + (3 * sizeX) / 4;
        const int32_t fromZ = flippedZ ? startZ + 15 + (-3 * sizeZ) / 4 : startZ + sizeZ / 4;
        const int32_t toZ = flippedZ ? startZ + 15 + sizeZ / -4 : startZ + (3 * sizeZ) / 4;

        int32_t total = 0;
        int32_t count = 0;
        for (int32_t x = fromX; x <= toX; ++x) {
            for (int32_t z = fromZ; z <= toZ; ++z) {
                total += terrain.getSolidHeight(x, z);
                ++count;
            }
        }

        return total / count;
    }

    using Position = std::tuple<int32_t, int32_t, int32_t>;
    using BlockIndex = std::map<Position, size_t>;

    struct Direction {
        int32_t mX;
        int32_t mZ;
    };

    // by weirdo_direction, the side a stair climbs to
    const std::array<Direction, 4> STAIR_SIDES = {{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};

    bool isStair(const BlockState &state) {
        return state.mStates.contains("weirdo_direction");
    }

    bool isFence(const BlockState &state) {
        return state.mName.size() > 6 && state.mName.compare(state.mName.size() - 6, 6, "_fence") == 0;
    }

    const ShipwreckStructure::PlacedBlock *at(const std::vector<ShipwreckStructure::PlacedBlock> &blocks,
                                              const BlockIndex &index, int32_t x, int32_t y, int32_t z) {
        const auto found = index.find({x, y, z});
        return found == index.end() ? nullptr : &blocks[found->second];
    }

    bool sameHalfStair(const ShipwreckStructure::PlacedBlock *block, const BlockState &stair) {
        return block != nullptr && isStair(block->mState)
               && block->mState.mStates.getByte("upside_down_bit", 0) == stair.mStates.getByte("upside_down_bit", 0);
    }

    // true when the stair at the side may turn towards it: nothing there continues the same run
    bool canTurn(const std::vector<ShipwreckStructure::PlacedBlock> &blocks, const BlockIndex &index,
                 const ShipwreckStructure::PlacedBlock &stair, const Direction &side) {
        const ShipwreckStructure::PlacedBlock *next = at(blocks, index, stair.x + side.mX, stair.y, stair.z + side.mZ);
        return !sameHalfStair(next, stair.mState)
               || next->mState.mStates.getInt("weirdo_direction", 0)
                          != stair.mState.mStates.getInt("weirdo_direction", 0);
    }

    std::string stairCorner(const std::vector<ShipwreckStructure::PlacedBlock> &blocks, const BlockIndex &index,
                            const ShipwreckStructure::PlacedBlock &stair) {
        const Direction facing = STAIR_SIDES[(size_t) stair.mState.mStates.getInt("weirdo_direction", 0)];
        const Direction left = {facing.mZ, -facing.mX};
        const auto turned = [&](const ShipwreckStructure::PlacedBlock *other, Direction &side) {
            if (!sameHalfStair(other, stair.mState))
                return false;
            side = STAIR_SIDES[(size_t) other->mState.mStates.getInt("weirdo_direction", 0)];
            return side.mX * facing.mX + side.mZ * facing.mZ == 0;
        };

        Direction side{};
        if (turned(at(blocks, index, stair.x + facing.mX, stair.y, stair.z + facing.mZ), side)
            && canTurn(blocks, index, stair, {-side.mX, -side.mZ}))
            return side.mX == left.mX && side.mZ == left.mZ ? "outer_left" : "outer_right";

        if (turned(at(blocks, index, stair.x - facing.mX, stair.y, stair.z - facing.mZ), side)
            && canTurn(blocks, index, stair, side))
            return side.mX == left.mX && side.mZ == left.mZ ? "inner_left" : "inner_right";

        return "none";
    }

    // the files keep shapes from the editor; the game works them out from the neighbours once the piece is down
    void connectShapes(std::vector<ShipwreckStructure::PlacedBlock> &blocks, StructureTerrain &terrain) {
        BlockIndex index;
        for (size_t i = 0; i < blocks.size(); ++i)
            index[{blocks[i].x, blocks[i].y, blocks[i].z}] = i;

        std::vector<ShipwreckStructure::PlacedBlock> shaped = blocks;
        for (ShipwreckStructure::PlacedBlock &block: shaped) {
            if (!isStair(block.mState))
                continue;

            Tag states = block.mState.mStates;
            states.putString("minecraft:corner", stairCorner(blocks, index, block));
            block.mState = BlockState(block.mState.mName, states);
        }

        // posts look at the turned stairs, and past the wreck at the terrain
        const auto connects = [&](const ShipwreckStructure::PlacedBlock &post, int32_t sideX, int32_t sideZ) {
            const ShipwreckStructure::PlacedBlock *next = at(shaped, index, post.x + sideX, post.y, post.z + sideZ);
            return StructureConnections::connects(
                    next != nullptr ? next->mState : terrain.getBlock(post.x + sideX, post.y, post.z + sideZ), sideX,
                    sideZ);
        };

        blocks = shaped;
        for (size_t i = 0; i < shaped.size(); ++i) {
            const ShipwreckStructure::PlacedBlock &post = shaped[i];
            if (isFence(post.mState))
                blocks[i].mState = StructureConnections::connected(post.mState, connects(post, 1, 0),
                                                                   connects(post, 0, -1), connects(post, 0, 1),
                                                                   connects(post, -1, 0));
        }
    }

    const char *markerLootTable(const std::string &marker) {
        if (marker == "mapChest")
            return "loot_tables/chests/shipwreck.json";
        if (marker == "supplyChest")
            return "loot_tables/chests/shipwrecksupply.json";
        if (marker == "treasureChest")
            return "loot_tables/chests/shipwrecktreasure.json";
        return nullptr;
    }
}

std::vector<ShipwreckStructure::PlacedBlock> ShipwreckStructure::build(const ChunkGenerator &generator, int32_t startX,
                                                                       int32_t startZ) {
    const int32_t cornerX = startX * 16;
    const int32_t cornerZ = startZ * 16;
    const int32_t surface = generator.getPreliminarySurfaceLevel(startX * 4, startZ * 4);
    const bool centerOnBeach = ShipwreckFeature::isBeachBiome(generator.pickBiome(cornerX + 8, surface, cornerZ + 8));
    const bool beached = ShipwreckFeature::isBeachBiome(
            generator.pickBiome(cornerX, centerOnBeach ? SEA_LEVEL : SEA_LEVEL - 32, cornerZ));

    std::mt19937 random = StructureFeature::createPostProcessRandom(generator.getSeed(), startX, startZ);
    const int32_t turns = FeatureRandom::nextInt(random, 4);
    int32_t piece = FeatureRandom::nextInt(random, PIECE_COUNT);
    if (beached)
        piece = FeatureRandom::nextInt(random, BEACHED_PIECE_COUNT);

    const std::pair<int32_t, int32_t> &pair = WOOD_PAIRS[(size_t) FeatureRandom::nextInt(random, WOOD_PAIR_COUNT)];
    StructureBlockTable::WoodSpecies wood;
    wood.mOak = SPECIES[(size_t) pair.first];
    wood.mDarkOak = SPECIES[(size_t) pair.second];

    std::vector<PlacedBlock> blocks;
    const std::shared_ptr<const StructureTemplate> shape = StructureTemplate::load(std::string("shipwreck/")
                                                                                   + PIECES[(size_t) piece] + ".nbt");
    if (shape == nullptr)
        return blocks;

    const bool quarter = (turns & 1) != 0;
    const int32_t sizeX = quarter ? shape->getSizeZ() : shape->getSizeX();
    const int32_t sizeZ = quarter ? shape->getSizeX() : shape->getSizeZ();

    StructureTerrain terrain(generator);
    const int32_t mean = meanHeight(terrain, cornerX, cornerZ, turns, sizeX, sizeZ);
    const int32_t originX = cornerX + ((turns == 1 || turns == 2) ? 15 : 0);
    const int32_t originZ = cornerZ + ((turns == 2 || turns == 3) ? 15 : 0);
    int32_t originY = mean + (piece < BEACHED_PIECE_COUNT ? -2 : -4);
    if (beached && originY < SEA_LEVEL)
        originY = SEA_LEVEL;

    const BlockState water = waterState();
    std::map<Position, size_t> chests;
    std::vector<StructureMarkers::Marker> markers;
    for (const StructureTemplate::BlockEntry &entry: shape->getBlocks()) {
        int32_t x = entry.x;
        int32_t z = entry.z;
        StructureTemplate::rotate(turns, x, z);
        x += originX;
        z += originZ;

        const std::string marker = entry.mData.getString("metadata", std::string());
        if (!marker.empty()) {
            markers.push_back({x, originY + entry.y, z, marker});
            continue;
        }

        PlacedBlock block;
        if (!StructureBlockTable::translate(shape->getPalette()[(size_t) entry.mPaletteIndex], turns, block.mState,
                                            wood))
            continue;

        block.x = x;
        block.z = z;
        block.y = originY + entry.y;
        // the game keeps the sea out of nothing: below sea level the empty blocks of a piece are water
        if (block.y < SEA_LEVEL && block.mState.mName == BlockNames::AIR)
            block.mState = water;
        block.mChest = entry.mData.getString("id", std::string()) == "minecraft:chest";
        if (block.mChest) {
            // placing a chest draws a seed of its own; only a marker replaces it
            block.mLootSeed = FeatureRandom::nextInt(random);
            chests[{block.x, block.y, block.z}] = blocks.size();
        }
        blocks.push_back(block);
    }

    const BlockState air("minecraft:air");
    for (const StructureMarkers::Marker &marker: StructureMarkers::inGameOrder(markers)) {
        PlacedBlock cleared;
        cleared.x = marker.x;
        cleared.y = marker.y;
        cleared.z = marker.z;
        cleared.mState = marker.y >= SEA_LEVEL ? air : water;
        blocks.push_back(cleared);

        const char *table = markerLootTable(marker.mText);
        if (table == nullptr)
            continue;

        const auto chest = chests.find({marker.x, marker.y - 1, marker.z});
        if (marker.mText != "mapChest" && FeatureRandom::nextInt(random, CHEST_KEPT_RANGE) < CHEST_KEPT_FROM) {
            PlacedBlock removed = cleared;
            removed.y = marker.y - 1;
            removed.mState = marker.y <= SEA_LEVEL ? water : air;
            blocks.push_back(removed);
            if (chest != chests.end())
                blocks[chest->second] = removed;
            continue;
        }

        if (chest == chests.end())
            continue;

        blocks[chest->second].mLootTable = table;
        blocks[chest->second].mLootSeed = FeatureRandom::nextInt(random);
    }

    connectShapes(blocks, terrain);
    return blocks;
}

void ShipwreckStructure::place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk) {
    const StructureFeature *shipwreck = StructureFeature::get("shipwreck");
    for (int32_t startX = chunk.getX() - MAX_REACH_CHUNKS; startX <= chunk.getX() + MAX_REACH_CHUNKS; ++startX) {
        for (int32_t startZ = chunk.getZ() - MAX_REACH_CHUNKS; startZ <= chunk.getZ() + MAX_REACH_CHUNKS; ++startZ) {
            if (!shipwreck->isStartChunk(generator, startX, startZ))
                continue;

            for (const PlacedBlock &block: build(generator, startX, startZ)) {
                if ((block.x >> 4) != chunk.getX() || (block.z >> 4) != chunk.getZ() || block.y < LevelChunk::MIN_Y
                    || block.y > LevelChunk::MAX_Y)
                    continue;

                chunk.setBlock(block.x & 15, block.y, block.z & 15, block.mState);
                if (!block.mChest)
                    continue;

                ChestBlockActor &chest = level.getBlockActors().getOrCreate<ChestBlockActor>(
                        Vector3i(block.x, block.y, block.z));
                if (!block.mLootTable.empty())
                    chest.setLootTable(block.mLootTable, block.mLootSeed);
            }
        }
    }
}
