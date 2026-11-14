#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Block/EndPortalFrameBlock.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/Dimension/Dimension.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/DimensionFactory.h"
#include "World/Level/LevelGen/Structure/StrongholdFeature.h"
#include "World/Level/LevelGen/Structure/StrongholdStructure.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"

#include <array>
#include <climits>
#include <cstdint>
#include <random>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// the references are the rules read in the 26.60 client, run by a separate model of them: no BDS world was dumped
namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    std::string serialize(const StrongholdStructure::Layout &layout) {
        std::string out;
        for (const StrongholdStructure::Piece &piece: layout.mPieces) {
            if (!out.empty())
                out += ';';
            const StructurePiece::Box &box = piece.getBox();
            out += std::string(StrongholdStructure::nameOf(piece.mType)) + ':' + std::to_string(box.mMinX) + ','
                   + std::to_string(box.mMinY) + ',' + std::to_string(box.mMinZ) + ',' + std::to_string(box.mMaxX) + ','
                   + std::to_string(box.mMaxY) + ',' + std::to_string(box.mMaxZ) + ','
                   + std::to_string(piece.getOrientation());
        }
        return out;
    }

    uint64_t fnv1a(const std::string &text) {
        uint64_t hash = 0xCBF29CE484222325ULL;
        for (const char character: text)
            hash = (hash ^ (uint8_t) character) * 0x100000001B3ULL;
        return hash;
    }

    struct GridCell {
        int64_t mSeed;
        int32_t mCellX;
        int32_t mCellZ;
        bool mValid;
        int32_t mX;
        int32_t mZ;
    };

    void checkGrid() {
        const GridCell cells[] = {{0, -1, 1, true, -138, 256},       {0, 1, 0, true, 321, 93},
                                  {0, 0, 0, false, 126, 71},         {0, -1, -1, false, -87, -51},
                                  {42, -1, 0, true, -82, 53},        {42, 1, 1, true, 305, 327},
                                  {42, 0, 1, false, 106, 289},       {123456789, 0, -1, true, 81, -145},
                                  {123456789, 1, -1, true, 290, -58}, {-987654321, 1, 0, true, 313, 55},
                                  {-987654321, 0, 0, false, 80, 129}};
        for (const GridCell &cell: cells) {
            StructureChunkPos start;
            const bool valid = StrongholdFeature::gridStronghold(cell.mSeed, cell.mCellX * 200 + 100,
                                                                 cell.mCellZ * 200 + 100, start);
            require(valid == cell.mValid && start.x == cell.mX && start.z == cell.mZ,
                    "Grid cell " + std::to_string(cell.mCellX) + ", " + std::to_string(cell.mCellZ) + " of seed "
                            + std::to_string(cell.mSeed) + " drew " + std::to_string(start.x) + ", "
                            + std::to_string(start.z));
        }

        StructureChunkPos origin;
        require(!StrongholdFeature::gridStronghold(42, 3, 4, origin), "A cell centred at the origin drew a start");
        require(StrongholdFeature::cellOf(-1) == -1 && StrongholdFeature::cellOf(199) == 0
                        && StrongholdFeature::cellOf(-200) == -1 && StrongholdFeature::cellOf(-201) == -2,
                "The cells are not floored");
    }

    struct LayoutCase {
        int64_t mSeed;
        int32_t mChunkX;
        int32_t mChunkZ;
        size_t mPieces;
        uint64_t mHash;
        StructurePiece::Box mPortalRoom;
        int32_t mPortalOrientation;
    };

    void checkLayouts() {
        const LayoutCase cases[] = {
                {42, -92, -27, 133, 0xf8bfc168c9182f5cULL, {-1444, 30, -408, -1434, 37, -393}, 0},
                {0, 30, -45, 193, 0x10d74166ef883640ULL, {506, -25, -731, 516, -18, -716}, 2},
                {123456789, -200, 150, 200, 0x5c522f4f29aaabd4ULL, {-3187, 17, 2395, -3177, 24, 2410}, 0}};
        for (const LayoutCase &expected: cases) {
            const StrongholdStructure::Layout layout =
                    StrongholdStructure::plan(expected.mSeed, expected.mChunkX, expected.mChunkZ);
            const std::string text = serialize(layout);
            const std::string where = "seed " + std::to_string(expected.mSeed) + " at " +
                                      std::to_string(expected.mChunkX) + ", " + std::to_string(expected.mChunkZ);
            require(layout.mPieces.size() == expected.mPieces && fnv1a(text) == expected.mHash,
                    "The layout of " + where + " differs: " + std::to_string(layout.mPieces.size()) + " pieces "
                            + text);

            const StrongholdStructure::Piece &start = layout.mPieces.front();
            require(start.mType == StrongholdStructure::Type::Start && start.getBox().mMinX == expected.mChunkX * 16 + 2
                            && start.getBox().mMaxX == expected.mChunkX * 16 + 6
                            && start.getBox().mMinZ == expected.mChunkZ * 16 + 2
                            && start.getBox().mMaxZ == expected.mChunkZ * 16 + 6
                            && start.getBox().mMaxY - start.getBox().mMinY == 10,
                    "The staircase of " + where + " is not in its start chunk");
            require(layout.mPieces[1].mType == StrongholdStructure::Type::FiveCrossing,
                    "The staircase of " + where + " does not open on a five way crossing");

            int32_t rooms = 0;
            for (const StrongholdStructure::Piece &piece: layout.mPieces) {
                require(piece.getBox().mMaxY <= layout.mBox.mMaxY && piece.getBox().mMinY >= layout.mBox.mMinY,
                        "A piece of " + where + " leaves the start's box");
                if (piece.mType != StrongholdStructure::Type::PortalRoom)
                    continue;
                ++rooms;
                require(piece.getBox() == expected.mPortalRoom && piece.getOrientation() == expected.mPortalOrientation,
                        "The portal room of " + where + " moved");
            }
            require(rooms == 1, "The stronghold of " + where + " has no single portal room");
            require(layout.mBox.mMaxY <= 53, "The stronghold of " + where + " reaches above the sea gap");

            require(StrongholdStructure::cachedPlan(expected.mSeed, expected.mChunkX, expected.mChunkZ)
                                    ->mPieces.size()
                            == layout.mPieces.size(),
                    "The kept layout of " + where + " differs");
        }
    }

    void checkVillageAnchors(int64_t seed) {
        const auto generator = DimensionFactory::createGenerator(DimensionType::Overworld, seed);
        const StructureFeature *village = StructureFeature::get("village");
        const StructureFeature *stronghold = StructureFeature::get("stronghold");
        require(village != nullptr && stronghold != nullptr, "The village or stronghold feature is missing");

        const auto starts = StrongholdFeature::villageStrongholds(*generator);
        const std::string where = "seed " + std::to_string(seed);
        require(starts->size() == 3, where + " found " + std::to_string(starts->size()) + " village strongholds");
        require(StrongholdFeature::villageStrongholds(*generator) == starts, where + " walked its spiral again");

        for (const StructureChunkPos &start: *starts) {
            const std::string at = where + " at " + std::to_string(start.x) + ", " + std::to_string(start.z);
            // the stronghold starts in the village's start chunk, the chunk its centre piece is laid from
            require(village->isStartChunk(*generator, start.x, start.z), at + " is under no village start");
            require(stronghold->isStartChunk(*generator, start.x, start.z), at + " is not a stronghold start");
            // the first spiral step is 40 to 55 chunks out, each window 8 chunks either side
            const int64_t squared = (int64_t) start.x * start.x + (int64_t) start.z * start.z;
            require(squared >= 28 * 28, at + " is closer than the spiral starts");

            // the staircase stands under the chunk's north west quarter, sunk below the sea
            const StrongholdStructure::Layout layout = StrongholdStructure::plan(seed, start.x, start.z);
            const StrongholdStructure::Piece &stairs = layout.mPieces.front();
            require(stairs.getBox().mMinX == start.x * 16 + 2 && stairs.getBox().mMinZ == start.z * 16 + 2
                            && stairs.getBox().mMaxY <= 53,
                    at + " has its staircase elsewhere");
        }

        // from the origin the search ends on the first ring: the nearest of the village starts and of the nine
        // cells around, the village cells left out
        StructureChunkPos nearest;
        require(stronghold->findNearest(*generator, 0, 0, nearest), where + " found no stronghold from the origin");
        int32_t best = INT_MAX;
        StructureChunkPos expected;
        for (const StructureChunkPos &start: *starts) {
            const int32_t squared = start.x * start.x + start.z * start.z;
            if (squared < best) {
                best = squared;
                expected = start;
            }
        }
        for (int32_t cellX = -1; cellX <= 1; ++cellX) {
            for (int32_t cellZ = -1; cellZ <= 1; ++cellZ) {
                bool villageCell = false;
                for (const StructureChunkPos &start: *starts)
                    villageCell = villageCell
                                  || (StrongholdFeature::cellOf(start.x) == cellX
                                      && StrongholdFeature::cellOf(start.z) == cellZ);
                StructureChunkPos grid;
                if (villageCell || !StrongholdFeature::gridStronghold(seed, cellX * 200 + 100, cellZ * 200 + 100, grid))
                    continue;
                const int32_t squared = grid.x * grid.x + grid.z * grid.z;
                if (squared < best) {
                    best = squared;
                    expected = grid;
                }
            }
        }
        require(nearest.x == expected.x && nearest.z == expected.z,
                where + " located " + std::to_string(nearest.x) + ", " + std::to_string(nearest.z));
    }

    using Piece = StrongholdStructure::Piece;
    using Type = StrongholdStructure::Type;

    // a portal room filling chunk 0, 0 for each orientation: its door on the chunk's edge
    Piece portalRoom(int32_t orientation) {
        const int32_t doors[4][2] = {{4, 0}, {15, 4}, {4, 15}, {0, 4}};
        const StructurePiece::Box box = StructurePiece::Box::orient(doors[orientation][0], 21, doors[orientation][1],
                                                                    -4, -1, 0, 11, 8, 16, orientation);
        return Piece(Type::PortalRoom, box, orientation, 7);
    }

    Vector3i worldOf(const Piece &piece, int32_t x, int32_t y, int32_t z) {
        return Vector3i(piece.worldX(x, z), piece.worldY(y), piece.worldZ(x, z));
    }

    // the twelve frames, the near row first, as the room lays them
    const int32_t FRAMES[12][3] = {{4, 3, 8},  {5, 3, 8},  {6, 3, 8},  {4, 3, 12}, {5, 3, 12}, {6, 3, 12},
                                   {3, 3, 9},  {3, 3, 10}, {3, 3, 11}, {7, 3, 9},  {7, 3, 10}, {7, 3, 11}};

    void checkPortalRooms() {
        for (int32_t orientation = 0; orientation < 4; ++orientation) {
            const std::string where = "orientation " + std::to_string(orientation);
            Level level("stronghold-portal-test", 2, 1);
            LevelChunk &chunk = level.insertChunk(LevelChunk(0, 0));
            const Piece room = portalRoom(orientation);
            require(room.getBox().mMinX >= 0 && room.getBox().mMaxX <= 15 && room.getBox().mMinZ >= 0
                            && room.getBox().mMaxZ <= 15,
                    "The room of " + where + " leaves its chunk");

            std::mt19937 random(1234U + (uint32_t) orientation);
            StrongholdStructure::postProcess(room, level, chunk, random);

            // every frame faces the portal's middle, the way the frames the eyes complete must
            const Vector3i middle = worldOf(room, 5, 3, 10);
            int32_t eyes = 0;
            for (const auto &frame: FRAMES) {
                const Vector3i at = worldOf(room, frame[0], frame[1], frame[2]);
                const BlockState state = level.getBlockState(at.x, at.y, at.z);
                require(state.mName == "minecraft:end_portal_frame", where + " misses a frame");
                const int32_t dx = at.x - middle.x;
                const int32_t dz = at.z - middle.z;
                const std::string expected = dx == -2 ? "east" : dx == 2 ? "west" : dz == -2 ? "south" : "north";
                require(state.mStates.getString("minecraft:cardinal_direction", "") == expected,
                        where + " has a frame facing " + state.mStates.getString("minecraft:cardinal_direction", ""));
                eyes += state.mStates.getByte("end_portal_eye_bit", 0) != 0 ? 1 : 0;
            }
            require(eyes < 12, where + " drew twelve eyes");
            const Vector3i inside = worldOf(room, 5, 3, 10);
            require(level.getBlockState(inside.x, inside.y, inside.z).mName == "minecraft:air",
                    where + " opened its portal without every eye");

            const Vector3i spawner = worldOf(room, 5, 3, 6);
            require(level.getBlockState(spawner.x, spawner.y, spawner.z).mName == "minecraft:mob_spawner",
                    where + " has no spawner");
            bool silverfish = false;
            for (const Tag &data: level.getBlockActors().saveChunk(0, 0)) {
                if (data.getString("id") == "MobSpawner" && data.getInt("x") == spawner.x
                    && data.getInt("y") == spawner.y && data.getInt("z") == spawner.z)
                    silverfish = data.getString("EntityIdentifier") == "minecraft:silverfish"
                                 && data.getShort("SpawnCount") == 4 && data.getShort("RequiredPlayerRange") == 16;
            }
            require(silverfish, where + " has no silverfish spawner data");

            const Vector3i lava = worldOf(room, 5, 1, 10);
            require(level.getBlockState(lava.x, lava.y, lava.z).mName == "minecraft:lava", where + " has no lava");
            require(!chunk.getPendingTicks().empty(), where + " left its liquids without a tick");

            // the missing eyes go in one by one: the portal opens with the last
            Vector3i last;
            for (const auto &frame: FRAMES) {
                const Vector3i at = worldOf(room, frame[0], frame[1], frame[2]);
                BlockState state = level.getBlockState(at.x, at.y, at.z);
                if (state.mStates.getByte("end_portal_eye_bit", 0) != 0)
                    continue;
                state.mStates.putByte("end_portal_eye_bit", 1);
                level.setBlockState(at.x, at.y, at.z, BlockState(state.mName, state.mStates));
                last = at;
            }
            require(EndPortalFrameBlock::tryCompletePortal(level, last, nullptr), where + " did not open its portal");
            for (int32_t z = 9; z <= 11; ++z) {
                for (int32_t x = 4; x <= 6; ++x) {
                    const Vector3i at = worldOf(room, x, 3, z);
                    require(level.getBlockState(at.x, at.y, at.z).mName == "minecraft:end_portal",
                            where + " has a hole in its portal");
                }
            }
        }

        // a room drawn with all twelve eyes opens at once
        Level level("stronghold-portal-test", 2, 1);
        LevelChunk &chunk = level.insertChunk(LevelChunk(0, 0));
        const Piece room = portalRoom(StructurePiece::SOUTH);
        std::array<bool, 12> all{};
        all.fill(true);
        StrongholdStructure::placePortal(room, level, chunk, all);
        const Vector3i inside = worldOf(room, 6, 3, 11);
        require(level.getBlockState(inside.x, inside.y, inside.z).mName == "minecraft:end_portal",
                "A room with every eye did not open");
        std::array<bool, 12> one = all;
        one[5] = false;
        Level other("stronghold-portal-test", 2, 1);
        LevelChunk &empty = other.insertChunk(LevelChunk(0, 0));
        StrongholdStructure::placePortal(room, other, empty, one);
        require(other.getBlockState(inside.x, inside.y, inside.z).mName == "minecraft:air",
                "A room missing an eye opened");
    }

    void checkRooms() {
        Level level("stronghold-room-test", 2, 1);
        LevelChunk &chunk = level.insertChunk(LevelChunk(0, 0));
        const StructurePiece::Box box =
                StructurePiece::Box::orient(4, 21, 0, -4, -1, 0, 14, 11, 15, StructurePiece::SOUTH);
        Piece library(Type::Library, box, StructurePiece::SOUTH, 6);
        library.mFlags[0] = true;
        std::mt19937 random(99);
        StrongholdStructure::postProcess(library, level, chunk, random);

        const Vector3i low = worldOf(library, 3, 3, 5);
        const Vector3i high = worldOf(library, 12, 8, 1);
        const BlockState lowChest = level.getBlockState(low.x, low.y, low.z);
        require(lowChest.mName == "minecraft:chest"
                        && lowChest.mStates.getString("minecraft:cardinal_direction", "") == "west",
                "The library's floor chest is not a chest facing west");
        require(level.getBlockState(high.x, high.y, high.z).mStates.getString("minecraft:cardinal_direction", "")
                        == "north",
                "The library's gallery chest does not face north");
        const ChestBlockActor *chest = level.getBlockActors().find<ChestBlockActor>(low);
        require(chest != nullptr, "The library chest has no actor");
        bool loot = false;
        for (const Tag &data: level.getBlockActors().saveChunk(0, 0))
            loot = loot || (data.getInt("x") == low.x && data.getInt("y") == low.y && data.getInt("z") == low.z
                            && data.getString("LootTable") == "loot_tables/chests/stronghold_library.json");
        require(loot, "The library chest has no stronghold loot");

        // the walls are bricks of the four kinds, the inside air
        const BlockState wall = level.getBlockState(library.worldX(0, 7), library.worldY(2), library.worldZ(0, 7));
        require(wall.mName == "minecraft:stone_bricks" || wall.mName == "minecraft:mossy_stone_bricks"
                        || wall.mName == "minecraft:cracked_stone_bricks"
                        || wall.mName == "minecraft:infested_stone_bricks",
                "A library wall is " + wall.mName);
        const BlockState ladder = level.getBlockState(library.worldX(10, 13), library.worldY(3),
                                                      library.worldZ(10, 13));
        require(ladder.mName == "minecraft:ladder" && ladder.mStates.getInt("facing_direction", -1) == 2,
                "The library ladder does not face north");
    }

    void checkPlacement() {
        const int64_t seed = 42;
        const auto generator = DimensionFactory::createGenerator(DimensionType::Overworld, seed);
        const StructureChunkPos start = StrongholdFeature::villageStrongholds(*generator)->front();
        const StrongholdStructure::Layout layout = StrongholdStructure::plan(seed, start.x, start.z);
        const Piece &stairs = layout.mPieces.front();

        Level level("stronghold-place-test", 2, seed);
        LevelChunk &chunk = level.insertChunk(LevelChunk(start.x, start.z));
        StrongholdStructure::place(*generator, level, chunk);
        const Vector3i landing = worldOf(stairs, 1, 1, 1);
        require(level.getBlockState(landing.x, landing.y, landing.z).mName == "minecraft:stone_bricks",
                "The staircase was not built in its start chunk");
        require(chunk.getStructureBounds().hasName("minecraft:stronghold"),
                "The start chunk does not know its stronghold");
    }
}

int main() {
    try {
        BlockPaletteRegistry::getInstance().initialize();
        BiomeChunkGenDataRegistry::initialize();
        checkGrid();
        checkLayouts();
        checkVillageAnchors(42);
        checkVillageAnchors(123456789);
        checkPortalRooms();
        checkRooms();
        checkPlacement();
    } catch (const std::exception &error) {
        std::clog << "Stronghold regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
