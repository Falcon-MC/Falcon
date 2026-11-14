#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Dimension/Dimension.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/DimensionFactory.h"
#include "World/Level/LevelGen/Structure/StrongholdFeature.h"
#include "World/Level/LevelGen/Structure/StrongholdStructure.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"

#include <climits>
#include <cstdint>
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
}

int main() {
    try {
        BlockPaletteRegistry::getInstance().initialize();
        BiomeChunkGenDataRegistry::initialize();
        checkGrid();
        checkLayouts();
        checkVillageAnchors(42);
        checkVillageAnchors(123456789);
    } catch (const std::exception &error) {
        std::clog << "Stronghold regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
