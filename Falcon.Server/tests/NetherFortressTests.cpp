#include "World/Level/Block/BlockPaletteRegistry.h"
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
}

int main() {
    try {
        BlockPaletteRegistry::getInstance().initialize();
        BiomeChunkGenDataRegistry::initialize();
        checkPieceFrame();
        checkPlacement();
    } catch (const std::exception &error) {
        std::clog << "Nether fortress regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
