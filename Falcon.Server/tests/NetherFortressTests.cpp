#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/Actor/MobSpawnerBlockActor.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/Nether/NetherGenerator.h"
#include "World/Level/LevelGen/Structure/NetherFortressFeature.h"
#include "World/Level/LevelGen/Structure/NetherFortressPieces.h"
#include "World/Level/LevelGen/Structure/NetherFortressStructure.h"
#include "World/Level/LevelGen/Structure/StructurePiece.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    using Piece = NetherFortressPieces::Piece;
    using Type = NetherFortressPieces::Type;
    using Box = StructurePiece::Box;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    const std::array<int64_t, 6> SEEDS = {0, 1, 12345, -987654321, 3141592653LL, 7710291837466LL};

    // the region random seeded the way the client seeds it for the nether complexes
    std::mt19937 regionRandom(int64_t seed, int32_t regionX, int32_t regionZ) {
        const int32_t mixed = (int32_t) ((uint32_t) seed - 1724254968u * (uint32_t) regionX
                                         - 245998635u * (uint32_t) regionZ + 30084232u);
        return std::mt19937((uint32_t) mixed);
    }

    void checkPieceFrame() {
        // a stairs room door at (100, 70, 200): the boxes the client computes for each orientation
        require(Box::orient(100, 70, 200, -2, 0, 0, 7, 11, 7, StructurePiece::SOUTH)
                        == Box{98, 70, 200, 104, 80, 206},
                "the south box differs");
        require(Box::orient(100, 70, 200, -2, 0, 0, 7, 11, 7, StructurePiece::WEST) == Box{94, 70, 198, 100, 80, 204},
                "the west box differs");
        require(Box::orient(100, 70, 200, -2, 0, 0, 7, 11, 7, StructurePiece::NORTH)
                        == Box{98, 70, 194, 104, 80, 200},
                "the north box differs");
        require(Box::orient(100, 70, 200, -2, 0, 0, 7, 11, 7, StructurePiece::EAST) == Box{100, 70, 198, 106, 80, 204},
                "the east box differs");

        const Box box{10, 40, 20, 14, 46, 28};
        const StructurePiece south(box, StructurePiece::SOUTH);
        const StructurePiece west(box, StructurePiece::WEST);
        const StructurePiece north(box, StructurePiece::NORTH);
        const StructurePiece east(box, StructurePiece::EAST);
        require(south.worldX(1, 3) == 11 && south.worldZ(1, 3) == 23, "south local to world differs");
        require(west.worldX(1, 3) == 11 && west.worldZ(1, 3) == 21, "west local to world differs");
        require(north.worldX(1, 3) == 11 && north.worldZ(1, 3) == 25, "north local to world differs");
        require(east.worldX(1, 3) == 13 && east.worldZ(1, 3) == 21, "east local to world differs");
        require(south.worldY(2) == 42 && StructurePiece(box, StructurePiece::NO_ORIENTATION).worldY(2) == 2,
                "y does not follow the box floor");

        // the client's stairs data tables
        const std::array<int32_t, 4> southStairs = {0, 1, 3, 2};
        const std::array<int32_t, 4> westStairs = {2, 3, 0, 1};
        const std::array<int32_t, 4> eastStairs = {2, 3, 1, 0};
        for (int32_t direction = 0; direction < 4; ++direction) {
            require(south.stairsDirection(direction) == southStairs[(size_t) direction], "south stairs differ");
            require(west.stairsDirection(direction) == westStairs[(size_t) direction], "west stairs differ");
            require(north.stairsDirection(direction) == direction, "north stairs differ");
            require(east.stairsDirection(direction) == eastStairs[(size_t) direction], "east stairs differ");
        }
    }

    void checkPieces(const NetherFortressStructure::Plan &plan, std::mt19937 &afterStartTest,
                     const std::string &label) {
        const std::vector<Piece> &pieces = plan.mPieces;
        require(!pieces.empty() && pieces.front().mType == Type::Start, label + ": no start piece first");

        // the start turns with the fourth region draw and sits 2 blocks into its chunk
        const Piece &start = pieces.front();
        require(start.getOrientation() == (int32_t) (afterStartTest() % 4u), label + ": start orientation differs");
        const Box &startBox = start.getBox();
        require(startBox.mMinX == plan.mStartX * 16 + 2 && startBox.mMinZ == plan.mStartZ * 16 + 2
                        && startBox.mMaxX - startBox.mMinX == 18 && startBox.mMaxZ - startBox.mMinZ == 18
                        && startBox.mMaxY - startBox.mMinY == 9,
                label + ": start box differs");

        std::map<Type, int32_t> counts;
        int32_t minY = startBox.mMinY;
        for (size_t i = 0; i < pieces.size(); ++i) {
            const Piece &piece = pieces[i];
            ++counts[piece.mType];
            minY = std::min(minY, piece.getBox().mMinY);
            require(piece.mDepth <= NetherFortressPieces::MAX_DEPTH + 1, label + ": a piece is too deep");
            require(piece.getOrientation() >= 0 && piece.getOrientation() <= 3, label + ": a piece is not turned");
            for (size_t j = i + 1; j < pieces.size(); ++j)
                require(!piece.getBox().intersects(pieces[j].getBox()), label + ": two pieces overlap");
        }

        // the fortress floor is moved between 48 and 70 when it fits
        require(minY >= NetherFortressStructure::SEA_LEVEL + 16, label + ": the fortress sits under its floor band");
        require(plan.mBox.mMinY == minY, label + ": the plan box misses a piece");

        require(counts[Type::Start] == 1, label + ": more than one start");
        require(counts[Type::BridgeCrossing] <= 4 && counts[Type::RoomCrossing] <= 4 && counts[Type::StairsRoom] <= 3
                        && counts[Type::MonsterThrone] <= 2 && counts[Type::CastleEntrance] <= 1,
                label + ": a bridge piece went past its cap");
        require(counts[Type::CastleSmallCorridorCrossing] <= 5 && counts[Type::CastleSmallCorridorRightTurn] <= 10
                        && counts[Type::CastleSmallCorridorLeftTurn] <= 10 && counts[Type::CastleCorridorStairs] <= 3
                        && counts[Type::CastleCorridorTBalcony] <= 2 && counts[Type::CastleStalkRoom] <= 2,
                label + ": a castle piece went past its cap");
        require(pieces.size() > 1, label + ": the start opened no bridge");
    }


    std::string nameAt(LevelChunk &chunk, int32_t x, int32_t y, int32_t z) {
        return chunk.getBlock(x & 15, y, z & 15).mName;
    }

    void checkPieceBlocks() {
        Level level("fortress-blocks-test", 2, 0, DimensionType::Nether);
        std::mt19937 random(11);

        // a throne opening south from (2, 60, 2): box (0, 60, 2) to (6, 67, 10)
        {
            LevelChunk chunk(0, 0);
            Piece throne(Type::MonsterThrone, Box::orient(2, 60, 2, -2, 0, 0, 7, 8, 9, StructurePiece::SOUTH),
                         StructurePiece::SOUTH, 1);
            require(throne.getBox() == Box{0, 60, 2, 6, 67, 10}, "the throne box differs");
            NetherFortressStructure::build(throne, level, chunk, random);
            require(nameAt(chunk, 3, 65, 7) == "minecraft:mob_spawner", "no spawner on the throne");
            const MobSpawnerBlockActor *spawner =
                    level.getBlockActors().find<MobSpawnerBlockActor>(Vector3i(3, 65, 7));
            require(spawner != nullptr && spawner->getEntityIdentifier() == "minecraft:blaze",
                    "the throne spawner does not spawn blazes");
            require(nameAt(chunk, 1, 60, 2) == "minecraft:nether_brick", "the throne floor differs");
            require(nameAt(chunk, 1, 66, 5) == "minecraft:nether_brick_fence", "the throne fence differs");
            require(nameAt(chunk, 0, 62, 2) == "minecraft:air", "the throne is not cleared");
            // the floor columns reach down through the empty chunk to y 2
            require(nameAt(chunk, 3, 2, 5) == "minecraft:nether_brick" && nameAt(chunk, 3, 1, 5) == "minecraft:air",
                    "the throne pillars do not reach the floor of the dimension");
        }

        // a stairs room turned east: local (5, 2, 5) lands at (minX + 5, y + 2, minZ + 5)
        {
            LevelChunk chunk(0, 0);
            Piece room(Type::StairsRoom, Box::orient(1, 70, 4, -2, 0, 0, 7, 11, 7, StructurePiece::EAST),
                       StructurePiece::EAST, 1);
            NetherFortressStructure::build(room, level, chunk, random);
            const Box &box = room.getBox();
            require(nameAt(chunk, box.mMinX + 5, 72, box.mMinZ + 5) == "minecraft:nether_brick",
                    "the turned room's step is misplaced");
            require(nameAt(chunk, box.mMinX + 4, 73, box.mMinZ + 6) == "minecraft:nether_brick_fence",
                    "the turned room's fence is misplaced");
        }

        // the stalk room: soul sand under the warts, its stairs turned with it
        for (int32_t orientation = 0; orientation < 4; ++orientation) {
            LevelChunk chunk(0, 0);
            Piece stalk(Type::CastleStalkRoom, Box{1, 40, 1, 13, 53, 13}, orientation, 3);
            NetherFortressStructure::build(stalk, level, chunk, random);
            const int32_t x = stalk.worldX(3, 4);
            const int32_t z = stalk.worldZ(3, 4);
            require(nameAt(chunk, x, 44, z) == "minecraft:soul_sand", "no soul sand in the stalk room");
            require(nameAt(chunk, x, 45, z) == "minecraft:nether_wart", "no nether wart in the stalk room");
            const BlockState &stairs = chunk.getBlock(stalk.worldX(6, 4), 45, stalk.worldZ(6, 4));
            require(stairs.mName == "minecraft:nether_brick_stairs"
                            && stairs.mStates.getInt("weirdo_direction", -1) == stalk.stairsDirection(3),
                    "the stalk room stairs are not turned with the room");
            require(nameAt(chunk, stalk.worldX(6, 6), 47, stalk.worldZ(6, 6)) == "minecraft:nether_brick_stairs",
                    "the stalk room stairs do not climb");
        }

        // a right turn that rolled its chest: data 4 faces west on every turn
        {
            LevelChunk chunk(0, 0);
            Piece turn(Type::CastleSmallCorridorRightTurn, Box{4, 50, 4, 8, 56, 8}, StructurePiece::NORTH, 4);
            turn.mNeedsChest = true;
            NetherFortressStructure::build(turn, level, chunk, random);
            const Vector3i at(turn.worldX(1, 3), 52, turn.worldZ(1, 3));
            const BlockState &chest = chunk.getBlock(at.x, at.y, at.z);
            require(chest.mName == "minecraft:chest"
                            && chest.mStates.getString("minecraft:cardinal_direction", "") == "west",
                    "the turn chest is missing or turned");
            ChestBlockActor *actor = level.getBlockActors().find<ChestBlockActor>(at);
            require(actor != nullptr
                            && actor->saveWithPosition().getString("LootTable") == NetherFortressStructure::LOOT_TABLE,
                    "the turn chest has no nether bridge loot");

            LevelChunk empty(0, 0);
            Piece plain = turn;
            plain.mNeedsChest = false;
            NetherFortressStructure::build(plain, level, empty, random);
            require(nameAt(empty, at.x, at.y, at.z) == "minecraft:air", "a turn without its roll got a chest");
        }

        // the entrance well: lava over a brick, in the middle of the hall
        {
            LevelChunk chunk(0, 0);
            Piece entrance(Type::CastleEntrance, Box{1, 40, 1, 13, 53, 13}, StructurePiece::SOUTH, 2);
            NetherFortressStructure::build(entrance, level, chunk, random);
            require(nameAt(chunk, 7, 45, 7) == "minecraft:lava", "no lava in the entrance well");
            require(nameAt(chunk, 7, 44, 7) == "minecraft:air" && nameAt(chunk, 7, 40, 7) == "minecraft:nether_brick",
                    "the entrance well shaft differs");
        }

        // an end filler crumbles the same in every chunk it reaches
        {
            Piece filler(Type::BridgeEndFiller, Box{14, 60, 2, 18, 69, 9}, StructurePiece::SOUTH, 5);
            filler.mSelfSeed = 424242;
            LevelChunk west(0, 0);
            LevelChunk east(1, 0);
            LevelChunk again(1, 0);
            NetherFortressStructure::build(filler, level, west, random);
            NetherFortressStructure::build(filler, level, east, random);
            NetherFortressStructure::build(filler, level, again, random);
            for (int32_t y = 60; y <= 65; ++y) {
                for (int32_t z = 2; z <= 9; ++z) {
                    for (int32_t x = 16; x <= 18; ++x)
                        require(nameAt(east, x, y, z) == nameAt(again, x, y, z), "the end filler is not stable");
                }
            }
            std::mt19937 own((uint32_t) filler.mSelfSeed);
            const int32_t first = (int32_t) (own() % 8u);
            for (int32_t z = 2; z <= 9; ++z)
                require((nameAt(west, 14, 63, z) == "minecraft:nether_brick") == (z - 2 <= first),
                        "the end filler's first row does not follow its own random");
        }
    }

    void checkChunkPlacement() {
        const int64_t seed = SEEDS[2];
        const NetherGenerator generator(seed);
        std::shared_ptr<const NetherFortressStructure::Plan> plan;
        for (int32_t regionX = -3; regionX <= 3 && plan == nullptr; ++regionX) {
            for (int32_t regionZ = -3; regionZ <= 3 && plan == nullptr; ++regionZ) {
                const StructureChunkPos candidate =
                        StructureFeature::get("fortress")->getCandidate(seed, regionX, regionZ);
                plan = NetherFortressStructure::plan(generator, candidate.x, candidate.z);
            }
        }
        require(plan != nullptr, "no fortress around the origin of the test seed");

        Level level("fortress-chunk-test", 2, seed, DimensionType::Nether);
        LevelChunk chunk(plan->mStartX, plan->mStartZ);
        NetherFortressStructure::place(generator, level, chunk);

        const Box &start = plan->mPieces.front().getBox();
        const StructurePiece &frame = plan->mPieces.front();
        require(nameAt(chunk, frame.worldX(9, 9), start.mMinY + 3, frame.worldZ(9, 9)) == "minecraft:nether_brick",
                "the start crossing has no deck");
        require(nameAt(chunk, frame.worldX(9, 9), start.mMinY + 6, frame.worldZ(9, 9)) == "minecraft:air",
                "the start crossing is not cleared");

        const std::vector<std::string> names =
                chunk.getStructureBounds().namesAt(frame.worldX(9, 9), start.mMinY + 3, frame.worldZ(9, 9));
        require(names.size() == 1 && names[0] == "minecraft:fortress", "the chunk does not record the fortress");
        require(chunk.getStructureBounds().namesAt(frame.worldX(9, 9), 2, frame.worldZ(9, 9)).empty(),
                "a block under the fortress is recorded inside it");
    }

    void checkPlacement() {
        const StructureFeature *fortress = StructureFeature::get("fortress");
        require(fortress != nullptr && fortress->getSpacing() == 30, "no fortress grid of 30 chunks");
        require(fortress->getDimension() == DimensionType::Nether, "the fortress is not a nether structure");

        int32_t found = 0;
        for (const int64_t seed: SEEDS) {
            const NetherGenerator generator(seed);
            for (int32_t regionX = -2; regionX <= 2; ++regionX) {
                for (int32_t regionZ = -2; regionZ <= 2; ++regionZ) {
                    std::mt19937 random = regionRandom(seed, regionX, regionZ);
                    const int32_t chunkX = regionX * 30 + (int32_t) (random() % 26u);
                    const int32_t chunkZ = regionZ * 30 + (int32_t) (random() % 26u);
                    const bool isFortress = random() % 6u < 2u;

                    const std::string label = "seed " + std::to_string(seed) + " region " + std::to_string(regionX)
                                              + "," + std::to_string(regionZ);
                    const StructureChunkPos candidate = fortress->getCandidate(seed, regionX, regionZ);
                    require(candidate.x == chunkX && candidate.z == chunkZ, label + ": candidate chunk differs");
                    require(fortress->isStartChunk(generator, chunkX, chunkZ) == isFortress,
                            label + ": the fortress roll differs");
                    require(!fortress->isStartChunk(generator, chunkX + 1, chunkZ),
                            label + ": a chunk off the candidate starts a fortress");

                    const std::shared_ptr<const NetherFortressStructure::Plan> plan =
                            NetherFortressStructure::plan(generator, chunkX, chunkZ);
                    require((plan != nullptr) == isFortress, label + ": a plan exists without its start");
                    if (plan == nullptr)
                        continue;

                    ++found;
                    checkPieces(*plan, random, label);

                    // planned again from scratch: the same pieces
                    std::mt19937 again = dynamic_cast<const NetherFortressFeature *>(fortress)->createFortressRandom(
                            seed, chunkX, chunkZ);
                    const std::vector<Piece> replay =
                            NetherFortressPieces::assemble(again, chunkX, chunkZ, NetherFortressStructure::SEA_LEVEL);
                    require(replay.size() == plan->mPieces.size(), label + ": a second plan differs in size");
                    for (size_t i = 0; i < replay.size(); ++i)
                        require(replay[i].mType == plan->mPieces[i].mType
                                        && replay[i].getBox() == plan->mPieces[i].getBox()
                                        && replay[i].getOrientation() == plan->mPieces[i].getOrientation(),
                                label + ": a second plan differs");

                    const std::vector<std::shared_ptr<const NetherFortressStructure::Plan>> near =
                            NetherFortressStructure::plansNear(generator, chunkX, chunkZ);
                    bool listed = false;
                    for (const auto &entry: near)
                        listed = listed || entry == plan;
                    require(listed, label + ": the start chunk does not see its fortress");
                }
            }
        }
        require(found > 0, "no seed had a fortress near the origin");
    }

    void checkLocate() {
        const StructureFeature *fortress = StructureFeature::get("fortress");
        for (const int64_t seed: SEEDS) {
            const NetherGenerator generator(seed);
            StructureChunkPos found;
            require(fortress->findNearest(generator, 0, 0, found), "/locate found no fortress");
            require(NetherFortressStructure::plan(generator, found.x, found.z) != nullptr,
                    "/locate reported a chunk without a fortress");
        }
    }
}

int main() {
    try {
        BlockPaletteRegistry::getInstance().initialize();
        BiomeChunkGenDataRegistry::initialize();
        checkPieceFrame();
        checkPlacement();
        checkPieceBlocks();
        checkChunkPlacement();
        checkLocate();
    } catch (const std::exception &error) {
        std::clog << "Nether fortress regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
