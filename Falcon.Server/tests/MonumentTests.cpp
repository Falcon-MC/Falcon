#include "World/Actor/Spawn/StructureSpawnOverrides.h"
#include "World/Level/LevelGen/Structure/MonumentPieces.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    using MonumentPieces::Layout;
    using MonumentPieces::Piece;
    using MonumentPieces::Type;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // the monument the game builds at seed 777 from chunk (47, -45): its box and turn were read from a server world
    void checkKnownStart() {
        const Layout layout = MonumentPieces::plan(777, 47, -45, 63);
        require(layout.mPieces.size() == 35, "the monument has " + std::to_string(layout.mPieces.size()) + " pieces");

        const Piece &building = layout.mPieces.front();
        require(building.mType == Type::Building, "the building comes first");
        require(building.getOrientation() == StructurePiece::NORTH, "the building faces north");
        const StructurePiece::Box &box = building.getBox();
        require(box.mMinX == 731 && box.mMinY == 39 && box.mMinZ == -741 && box.mMaxX == 788 && box.mMaxY == 61
                        && box.mMaxZ == -684,
                "the building box differs");

        require(layout.mPieces[1].mType == Type::Entry && layout.mPieces[2].mType == Type::Core,
                "the entry and the core follow the building");
        const size_t count = layout.mPieces.size();
        require(layout.mPieces[count - 3].mType == Type::Wing && layout.mPieces[count - 2].mType == Type::Wing
                        && layout.mPieces[count - 1].mType == Type::Penthouse,
                "the wings and the penthouse come last");
        require(layout.mPieces[count - 3].mDesign != layout.mPieces[count - 2].mDesign,
                "the wings take both designs");
        require(!layout.mPieces[count - 3].mSecondWing && layout.mPieces[count - 2].mSecondWing,
                "the second wing is marked");

        for (const Piece &piece: layout.mPieces) {
            const StructurePiece::Box &inner = piece.getBox();
            require(inner.mMinX >= box.mMinX && inner.mMaxX <= box.mMaxX && inner.mMinZ >= box.mMinZ
                            && inner.mMaxZ <= box.mMaxZ && inner.mMinY >= box.mMinY && inner.mMaxY <= box.mMaxY,
                    "a piece leaves the building");
        }
        for (const MonumentPieces::Room &room: layout.mRooms)
            require(room.mClaimed, "room " + std::to_string(room.mIndex) + " has no piece");
    }

    void checkDeterminism() {
        for (const int64_t seed: {0LL, 1LL, -987654321LL, 7710291837466LL}) {
            const Layout first = MonumentPieces::plan(seed, 12, -30, 63);
            const Layout second = MonumentPieces::plan(seed, 12, -30, 63);
            require(first.mPieces.size() == second.mPieces.size(), "the plan changes between runs");
            for (size_t index = 0; index < first.mPieces.size(); ++index) {
                require(first.mPieces[index].getBox() == second.mPieces[index].getBox()
                                && first.mPieces[index].mType == second.mPieces[index].mType,
                        "a piece changes between runs");
            }
        }
    }

    void checkGuardianSpawns() {
        const std::vector<SpawnRule> *rules = StructureSpawnOverrides::forStructure("minecraft:monument", "monster");
        require(rules != nullptr && rules->size() == 1, "the monument has no monster override");
        const SpawnRule &rule = rules->front();
        require(rule.mIdentifier == "minecraft:guardian", "the monument spawns " + rule.mIdentifier);
        const SpawnCondition &condition = rule.mConditions.front();
        require(condition.mUnderwater && condition.mWeight == 1, "the guardians spawn out of water");
        require(condition.mHerds.size() == 1 && condition.mHerds.front().mMinSize == 2 && condition.mHerds.front().mMaxSize == 4,
                "the guardian herd differs");
    }
}

int main() {
    try {
        checkKnownStart();
        checkDeterminism();
        checkGuardianSpawns();
    } catch (const std::exception &error) {
        std::cerr << "monument: " << error.what() << '\n';
        return 1;
    }
    std::cout << "monument: ok\n";
    return 0;
}
