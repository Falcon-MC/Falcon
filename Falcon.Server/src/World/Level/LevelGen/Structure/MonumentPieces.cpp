#include "World/Level/LevelGen/Structure/MonumentPieces.h"

#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"
#include "World/Level/LevelGen/Random/GameRandom.h"
#include "World/Level/LevelGen/Structure/StructurePieceWriter.h"

#include <algorithm>
#include <utility>

using MonumentPieces::Layout;
using MonumentPieces::Piece;
using MonumentPieces::Room;
using MonumentPieces::Type;

namespace {
    using MonumentPieces::DOWN;
    using MonumentPieces::EAST;
    using MonumentPieces::NO_ROOM;
    using MonumentPieces::NORTH;
    using MonumentPieces::SOUTH;
    using MonumentPieces::UP;
    using MonumentPieces::WEST;

    const std::array<int32_t, 6> OPPOSITE = {UP, DOWN, SOUTH, NORTH, EAST, WEST};

    const int32_t GRID_ROOMS = 75;
    const int32_t SOURCE_INDEX = 2;
    const int32_t PENTHOUSE_DOOR_INDEX = 52;
    const int32_t LEFT_WING_DOOR_INDEX = 25;
    const int32_t RIGHT_WING_DOOR_INDEX = 29;
    const int32_t LEFT_WING_INDEX = 1001;
    const int32_t RIGHT_WING_INDEX = 1002;
    const int32_t PENTHOUSE_INDEX = 1003;

    int32_t gridIndex(int32_t x, int32_t y, int32_t z) {
        return x + 5 * z + 25 * y;
    }

    int32_t addRoom(std::vector<Room> &rooms, int32_t index) {
        Room room;
        room.mIndex = index;
        rooms.push_back(room);
        return (int32_t) rooms.size() - 1;
    }

    void connect(std::vector<Room> &rooms, int32_t from, int32_t face, int32_t to) {
        rooms[(size_t) from].mConnections[(size_t) face] = to;
        rooms[(size_t) to].mConnections[(size_t) OPPOSITE[(size_t) face]] = from;
    }

    void updateOpenings(Room &room) {
        for (size_t face = 0; face < 6; ++face)
            room.mOpenings[face] = room.mConnections[face] != NO_ROOM;
    }

    const Room &linked(const std::vector<Room> &rooms, const Room &room, int32_t face) {
        return rooms[(size_t) room.mConnections[(size_t) face]];
    }

    Room &linked(std::vector<Room> &rooms, const Room &room, int32_t face) {
        return rooms[(size_t) room.mConnections[(size_t) face]];
    }

    bool hasLink(const Room &room, int32_t face) {
        return room.mConnections[(size_t) face] != NO_ROOM;
    }

    bool findSource(std::vector<Room> &rooms, int32_t at, int32_t scanIndex) {
        Room &room = rooms[(size_t) at];
        if (room.mSource)
            return true;

        room.mScanIndex = scanIndex;
        for (size_t face = 0; face < 6; ++face) {
            const int32_t next = room.mConnections[face];
            if (next != NO_ROOM && room.mOpenings[face] && rooms[(size_t) next].mScanIndex != scanIndex
                && findSource(rooms, next, scanIndex))
                return true;
        }
        return false;
    }

    int32_t toOrientation(int32_t facing) {
        // the game draws the facing in north, south, west, east order
        static const int32_t ORIENTATIONS[4] = {StructurePiece::NORTH, StructurePiece::SOUTH, StructurePiece::WEST,
                                                StructurePiece::EAST};
        return ORIENTATIONS[facing];
    }

    // a room's box in the building's frame: cells of 8 by 4 by 8, the depth turned with the building
    StructurePiece::Box roomBox(int32_t index, int32_t orientation, int32_t width, int32_t height, int32_t depth) {
        const int32_t cellX = index % 5;
        const int32_t cellZ = index / 5 % 5;
        const int32_t cellY = index / 25;
        const bool alongZ = orientation == StructurePiece::NORTH || orientation == StructurePiece::SOUTH;

        StructurePiece::Box box;
        box.mMaxX = 8 * (alongZ ? width : depth) - 1;
        box.mMaxY = 4 * height - 1;
        box.mMaxZ = 8 * (alongZ ? depth : width) - 1;
        switch (orientation) {
            case StructurePiece::WEST:
                box.move(1 - 8 * (depth + cellZ), 4 * cellY, 8 * cellX);
                break;
            case StructurePiece::SOUTH:
                box.move(8 * cellX, 4 * cellY, 8 * cellZ);
                break;
            case StructurePiece::NORTH:
                box.move(8 * cellX, 4 * cellY, 1 - 8 * (depth + cellZ));
                break;
            default:
                box.move(8 * cellZ, 4 * cellY, 8 * cellX);
                break;
        }
        return box;
    }

    StructurePiece::Box cornerBox(const Piece &building, int32_t x0, int32_t y0, int32_t z0, int32_t x1, int32_t y1,
                                  int32_t z1) {
        const int32_t firstX = building.worldX(x0, z0);
        const int32_t firstZ = building.worldZ(x0, z0);
        const int32_t lastX = building.worldX(x1, z1);
        const int32_t lastZ = building.worldZ(x1, z1);
        StructurePiece::Box box;
        box.mMinX = std::min(firstX, lastX);
        box.mMinY = building.worldY(y0);
        box.mMinZ = std::min(firstZ, lastZ);
        box.mMaxX = std::max(firstX, lastX);
        box.mMaxY = building.worldY(y1);
        box.mMaxZ = std::max(firstZ, lastZ);
        return box;
    }

    // the room shapes the game tries on every unclaimed room, the first fitting wins
    bool fitDoubleXY(std::vector<Room> &rooms, Room &room, bool create) {
        if (!room.mOpenings[EAST] || !room.mOpenings[UP])
            return false;
        Room &east = linked(rooms, room, EAST);
        if (east.mClaimed || linked(rooms, room, UP).mClaimed || !east.mOpenings[UP]
            || linked(rooms, east, UP).mClaimed)
            return false;
        if (create) {
            room.mClaimed = true;
            east.mClaimed = true;
            linked(rooms, room, UP).mClaimed = true;
            linked(rooms, east, UP).mClaimed = true;
        }
        return true;
    }

    bool fitDoubleYZ(std::vector<Room> &rooms, Room &room, bool create) {
        if (!room.mOpenings[NORTH] || !room.mOpenings[UP])
            return false;
        Room &north = linked(rooms, room, NORTH);
        if (north.mClaimed || linked(rooms, room, UP).mClaimed || !north.mOpenings[UP]
            || linked(rooms, north, UP).mClaimed)
            return false;
        if (create) {
            room.mClaimed = true;
            north.mClaimed = true;
            linked(rooms, room, UP).mClaimed = true;
            linked(rooms, north, UP).mClaimed = true;
        }
        return true;
    }

    bool fitPair(std::vector<Room> &rooms, Room &room, int32_t face, bool create) {
        if (!room.mOpenings[(size_t) face] || linked(rooms, room, face).mClaimed)
            return false;
        if (create) {
            room.mClaimed = true;
            linked(rooms, room, face).mClaimed = true;
        }
        return true;
    }

    Piece roomPiece(Type type, int32_t slot, const Room &room, int32_t orientation, int32_t width, int32_t height,
                    int32_t depth) {
        Piece piece(type, roomBox(room.mIndex, orientation, width, height, depth), orientation);
        piece.mRoom = slot;
        return piece;
    }
}

Layout MonumentPieces::plan(int64_t seed, int32_t chunkX, int32_t chunkZ, int32_t seaLevel) {
    Layout layout;
    std::vector<Room> &rooms = layout.mRooms;

    std::mt19937 random((uint32_t) seed);
    const float scaleX = GameRandom::nextFloat(random);
    const float scaleZ = GameRandom::nextFloat(random);
    // the world seed mixed with both draws scaled by the chunk, truncated as the game does
    const uint32_t monumentSeed = (uint32_t) seed ^ (uint32_t) (int64_t) (scaleZ * (float) chunkZ)
                                  ^ (uint32_t) (int64_t) (scaleX * (float) chunkX);
    random.seed(monumentSeed);

    const int32_t orientation = toOrientation(FeatureRandom::nextInt(random, 4));
    StructurePiece::Box buildingBox;
    buildingBox.mMinX = chunkX * 16 - 21;
    buildingBox.mMinY = seaLevel - 24;
    buildingBox.mMinZ = chunkZ * 16 - 21;
    buildingBox.mMaxX = chunkX * 16 + 36;
    buildingBox.mMaxY = seaLevel - 2;
    buildingBox.mMaxZ = chunkZ * 16 + 36;
    layout.mPieces.emplace_back(Type::Building, buildingBox, orientation);

    std::array<int32_t, GRID_ROOMS> slots;
    slots.fill(NO_ROOM);
    for (int32_t x = 0; x < 5; ++x) {
        for (int32_t z = 0; z < 4; ++z)
            slots[(size_t) gridIndex(x, 0, z)] = addRoom(rooms, gridIndex(x, 0, z));
    }
    for (int32_t x = 0; x < 5; ++x) {
        for (int32_t z = 0; z < 4; ++z)
            slots[(size_t) gridIndex(x, 1, z)] = addRoom(rooms, gridIndex(x, 1, z));
    }
    for (int32_t x = 1; x <= 3; ++x) {
        for (int32_t z = 0; z < 2; ++z)
            slots[(size_t) gridIndex(x, 2, z)] = addRoom(rooms, gridIndex(x, 2, z));
    }

    // faces walked up, down, north, south, west, east; a step along z joins through the opposite face
    static const int32_t STEPS[6][4] = {{0, 1, 0, UP},     {0, -1, 0, DOWN}, {0, 0, -1, NORTH},
                                        {0, 0, 1, SOUTH}, {-1, 0, 0, WEST}, {1, 0, 0, EAST}};
    for (int32_t x = 0; x < 5; ++x) {
        for (int32_t z = 0; z < 5; ++z) {
            for (int32_t y = 0; y < 3; ++y) {
                const int32_t from = slots[(size_t) gridIndex(x, y, z)];
                if (from == NO_ROOM)
                    continue;
                for (const int32_t *step: STEPS) {
                    const int32_t nextX = x + step[0];
                    const int32_t nextY = y + step[1];
                    const int32_t nextZ = z + step[2];
                    if ((uint32_t) nextX >= 5 || (uint32_t) nextZ >= 5 || (uint32_t) nextY >= 3)
                        continue;
                    const int32_t to = slots[(size_t) gridIndex(nextX, nextY, nextZ)];
                    if (to == NO_ROOM)
                        continue;
                    connect(rooms, from, step[2] != 0 ? OPPOSITE[(size_t) step[3]] : step[3], to);
                }
            }
        }
    }

    const int32_t penthouse = addRoom(rooms, PENTHOUSE_INDEX);
    const int32_t leftWing = addRoom(rooms, LEFT_WING_INDEX);
    const int32_t rightWing = addRoom(rooms, RIGHT_WING_INDEX);
    connect(rooms, slots[PENTHOUSE_DOOR_INDEX], UP, penthouse);
    connect(rooms, slots[LEFT_WING_DOOR_INDEX], SOUTH, leftWing);
    connect(rooms, slots[RIGHT_WING_DOOR_INDEX], SOUTH, rightWing);
    rooms[(size_t) penthouse].mClaimed = true;
    rooms[(size_t) leftWing].mClaimed = true;
    rooms[(size_t) rightWing].mClaimed = true;
    const int32_t source = slots[SOURCE_INDEX];
    rooms[(size_t) source].mSource = true;

    // the core takes the 2x2x2 block east and north of one of the third row's rooms
    const int32_t core = slots[(size_t) gridIndex(FeatureRandom::nextInt(random, 4), 0, 2)];
    {
        Room &room = rooms[(size_t) core];
        Room &east = linked(rooms, room, EAST);
        Room &north = linked(rooms, room, NORTH);
        Room &eastNorth = linked(rooms, east, NORTH);
        room.mClaimed = true;
        east.mClaimed = true;
        north.mClaimed = true;
        eastNorth.mClaimed = true;
        linked(rooms, room, UP).mClaimed = true;
        linked(rooms, east, UP).mClaimed = true;
        linked(rooms, north, UP).mClaimed = true;
        linked(rooms, eastNorth, UP).mClaimed = true;
    }

    std::vector<int32_t> order;
    for (int32_t index = 0; index < GRID_ROOMS; ++index) {
        const int32_t slot = slots[(size_t) index];
        if (slot == NO_ROOM)
            continue;
        updateOpenings(rooms[(size_t) slot]);
        order.push_back(slot);
    }
    updateOpenings(rooms[(size_t) penthouse]);

    // shuffled with its own twister on the monument seed, the bound one short of the swapped slot
    std::mt19937 shuffle(monumentSeed);
    for (size_t index = order.size() - 1; index > 0; --index)
        std::swap(order[(size_t) FeatureRandom::nextInt(shuffle, (int32_t) index)], order[index]);

    // every room closes up to two of its openings, as long as both sides still reach the entrance
    int32_t scanIndex = 1;
    for (const int32_t at: order) {
        int32_t closed = 0;
        int32_t tries = 0;
        while (closed <= 1 && tries <= 4) {
            const int32_t face = FeatureRandom::nextInt(random, 6);
            ++tries;
            Room &room = rooms[(size_t) at];
            if (!room.mOpenings[(size_t) face])
                continue;

            const int32_t next = room.mConnections[(size_t) face];
            const size_t back = (size_t) OPPOSITE[(size_t) face];
            room.mOpenings[(size_t) face] = false;
            rooms[(size_t) next].mOpenings[back] = false;
            const int32_t first = scanIndex;
            if (findSource(rooms, at, first)) {
                scanIndex = first + 2;
                if (findSource(rooms, next, first + 1)) {
                    ++closed;
                    continue;
                }
            } else {
                scanIndex = first + 1;
            }
            rooms[(size_t) at].mOpenings[(size_t) face] = true;
            rooms[(size_t) next].mOpenings[back] = true;
        }
    }
    order.push_back(penthouse);
    order.push_back(leftWing);
    order.push_back(rightWing);
    rooms[(size_t) source].mClaimed = true;

    std::vector<Piece> &pieces = layout.mPieces;
    pieces.push_back(roomPiece(Type::Entry, source, rooms[(size_t) source], orientation, 1, 1, 1));
    pieces.push_back(roomPiece(Type::Core, core, rooms[(size_t) core], orientation, 2, 2, 2));
    for (const int32_t at: order) {
        Room &room = rooms[(size_t) at];
        if (room.mClaimed || room.mIndex >= GRID_ROOMS)
            continue;

        if (fitDoubleXY(rooms, room, true)) {
            pieces.push_back(roomPiece(Type::DoubleXY, at, room, orientation, 2, 2, 1));
        } else if (fitDoubleYZ(rooms, room, true)) {
            pieces.push_back(roomPiece(Type::DoubleYZ, at, room, orientation, 1, 2, 2));
        } else if (fitPair(rooms, room, NORTH, true)) {
            pieces.push_back(roomPiece(Type::DoubleZ, at, room, orientation, 1, 1, 2));
        } else if (fitPair(rooms, room, EAST, true)) {
            pieces.push_back(roomPiece(Type::DoubleX, at, room, orientation, 2, 1, 1));
        } else if (fitPair(rooms, room, UP, true)) {
            pieces.push_back(roomPiece(Type::DoubleY, at, room, orientation, 1, 2, 1));
        } else if (std::none_of(room.mOpenings.begin() + 1, room.mOpenings.end(), [](bool open) { return open; })) {
            room.mClaimed = true;
            pieces.push_back(roomPiece(Type::SimpleTop, at, room, orientation, 1, 1, 1));
        } else {
            room.mClaimed = true;
            Piece piece = roomPiece(Type::Simple, at, room, orientation, 1, 1, 1);
            piece.mDesign = FeatureRandom::nextInt(random, 3);
            pieces.push_back(piece);
        }
    }

    const Piece &building = pieces.front();
    const int32_t offsetX = building.worldX(9, 22);
    const int32_t offsetY = building.getBox().mMinY;
    const int32_t offsetZ = building.worldZ(9, 22);
    for (size_t index = 1; index < pieces.size(); ++index)
        pieces[index].getBox().move(offsetX, offsetY, offsetZ);

    const int32_t design = GameRandom::nextInt(random) & 1;
    Piece firstWing(Type::Wing, cornerBox(building, 1, 1, 1, 23, 8, 21), orientation);
    firstWing.mDesign = design;
    Piece secondWing(Type::Wing, cornerBox(building, 34, 1, 1, 56, 8, 21), orientation);
    secondWing.mDesign = design ^ 1;
    secondWing.mSecondWing = true;
    Piece top(Type::Penthouse, cornerBox(building, 22, 13, 22, 35, 17, 35), orientation);
    pieces.push_back(firstWing);
    pieces.push_back(secondWing);
    pieces.push_back(top);
    return layout;
}

namespace {
    struct Blocks {
        BlockState mGray = VanillaBlocks::PRISMARINE().toBlockState();
        BlockState mLight = VanillaBlocks::PRISMARINE_BRICKS().toBlockState();
        BlockState mBlack = VanillaBlocks::DARK_PRISMARINE().toBlockState();
        BlockState mLamp = VanillaBlocks::SEA_LANTERN().toBlockState();
        BlockState mWater = VanillaBlocks::WATER().toBlockState();
        BlockState mAir = VanillaBlocks::AIR().toBlockState();
        BlockState mGold = VanillaBlocks::GOLD_BLOCK().toBlockState();
        BlockState mSponge = VanillaBlocks::WET_SPONGE().toBlockState();
    };

    const Blocks &blocks() {
        static const Blocks shared;
        return shared;
    }

    class Builder {
    public:
        Builder(const Piece &piece, Level &level, LevelChunk &chunk, int32_t seaLevel)
                : mPiece(piece), mChunk(chunk), mWriter(level, chunk, piece), mSeaLevel(seaLevel) {}

        void box(int32_t minX, int32_t minY, int32_t minZ, int32_t maxX, int32_t maxY, int32_t maxZ,
                 const BlockState &state) {
            mWriter.generateBox(minX, minY, minZ, maxX, maxY, maxZ, state, state, false);
        }

        void place(const BlockState &state, int32_t x, int32_t y, int32_t z) {
            mWriter.placeBlock(state, x, y, z);
        }

        // water below the sea level, air above it
        void water(int32_t minX, int32_t minY, int32_t minZ, int32_t maxX, int32_t maxY, int32_t maxZ) {
            for (int32_t y = minY; y <= maxY; ++y) {
                const BlockState &state = mPiece.worldY(y) < mSeaLevel ? blocks().mWater : blocks().mAir;
                for (int32_t x = minX; x <= maxX; ++x) {
                    for (int32_t z = minZ; z <= maxZ; ++z)
                        place(state, x, y, z);
                }
            }
        }

        // replaces only water: a ceiling under an open sky of rooms
        void fillWater(int32_t minX, int32_t maxX, int32_t y, int32_t minZ, int32_t maxZ, const BlockState &state) {
            for (int32_t x = minX; x <= maxX; ++x) {
                for (int32_t z = minZ; z <= maxZ; ++z) {
                    const BlockState *current = mWriter.getBlock(x, y, z);
                    if (current != nullptr && current->mName == BlockNames::WATER)
                        place(state, x, y, z);
                }
            }
        }

        void pillar(int32_t x, int32_t z) {
            place(blocks().mLight, x, 0, z);
            mWriter.fillColumnDown(blocks().mLight, x, -1, z);
        }

        // a floor of an upper room, with the shaft down left open
        void floor(int32_t x, int32_t z, bool open) {
            const Blocks &k = blocks();
            if (!open) {
                box(x, 0, z, x + 7, 0, z + 7, k.mGray);
                return;
            }
            box(x, 0, z, x + 2, 0, z + 7, k.mGray);
            box(x + 5, 0, z, x + 7, 0, z + 7, k.mGray);
            box(x + 3, 0, z, x + 4, 0, z + 2, k.mGray);
            box(x + 3, 0, z + 5, x + 4, 0, z + 7, k.mGray);
            box(x + 3, 0, z + 2, x + 4, 0, z + 2, k.mLight);
            box(x + 3, 0, z + 5, x + 4, 0, z + 5, k.mLight);
            box(x + 2, 0, z + 3, x + 2, 0, z + 4, k.mLight);
            box(x + 5, 0, z + 3, x + 5, 0, z + 4, k.mLight);
        }

        // the columns between the two corners meet the chunk
        bool meets(int32_t x0, int32_t z0, int32_t x1, int32_t z1) const {
            const int32_t firstX = mPiece.worldX(x0, z0);
            const int32_t firstZ = mPiece.worldZ(x0, z0);
            const int32_t lastX = mPiece.worldX(x1, z1);
            const int32_t lastZ = mPiece.worldZ(x1, z1);
            const int32_t chunkMinX = mChunk.getX() * 16;
            const int32_t chunkMinZ = mChunk.getZ() * 16;
            return std::max(firstX, lastX) >= chunkMinX && std::min(firstX, lastX) <= chunkMinX + 15
                   && std::max(firstZ, lastZ) >= chunkMinZ && std::min(firstZ, lastZ) <= chunkMinZ + 15;
        }

    private:
        const Piece &mPiece;
        LevelChunk &mChunk;
        StructurePieceWriter mWriter;
        int32_t mSeaLevel;
    };

    bool touchesChunk(const StructurePiece::Box &box, const LevelChunk &chunk) {
        return box.mMaxX >= chunk.getX() * 16 && box.mMinX <= chunk.getX() * 16 + 15 && box.mMaxZ >= chunk.getZ() * 16
               && box.mMinZ <= chunk.getZ() * 16 + 15;
    }

    void buildWing(Builder &b, bool flipped, int32_t offset) {
        const Blocks &k = blocks();
        if (!b.meets(offset, 0, offset + 23, 20))
            return;

        b.box(offset, 0, 0, offset + 24, 0, 20, k.mGray);
        b.water(offset, 1, 0, offset + 24, 10, 20);
        for (int32_t i = 0; i < 4; ++i) {
            b.box(offset + i, i + 1, i, offset + i, i + 1, 20, k.mLight);
            b.box(offset + i + 7, i + 5, i + 7, offset + i + 7, i + 5, 20, k.mLight);
            b.box(offset + 17 - i, i + 5, i + 7, offset + 17 - i, i + 5, 20, k.mLight);
            b.box(offset + 24 - i, i + 1, i, offset + 24 - i, i + 1, 20, k.mLight);
            b.box(offset + i + 1, i + 1, i, offset + 23 - i, i + 1, i, k.mLight);
            b.box(offset + i + 8, i + 5, i + 7, offset + 16 - i, i + 5, i + 7, k.mLight);
        }
        b.box(offset + 4, 4, 4, offset + 6, 4, 20, k.mGray);
        b.box(offset + 7, 4, 4, offset + 17, 4, 6, k.mGray);
        b.box(offset + 18, 4, 4, offset + 20, 4, 20, k.mGray);
        b.box(offset + 11, 8, 11, offset + 13, 8, 20, k.mGray);
        b.place(k.mLight, offset + 12, 9, 12);
        b.place(k.mLight, offset + 12, 9, 15);
        b.place(k.mLight, offset + 12, 9, 18);

        const int32_t inner = offset + (flipped ? 19 : 5);
        const int32_t outer = offset + (flipped ? 5 : 19);
        for (int32_t z = 20; z >= 5; z -= 3)
            b.place(k.mLight, inner, 5, z);
        for (int32_t z = 19; z >= 7; z -= 3)
            b.place(k.mLight, outer, 5, z);
        for (int32_t i = 0; i < 4; ++i)
            b.place(k.mLight, flipped ? offset + 24 - (17 - i * 3) : offset + 17 - i * 3, 5, 5);
        b.place(k.mLight, outer, 5, 5);
        b.box(offset + 11, 1, 12, offset + 13, 7, 12, k.mGray);
        b.box(offset + 12, 1, 11, offset + 12, 7, 13, k.mGray);
    }

    void buildEntranceArches(Builder &b) {
        const Blocks &k = blocks();
        if (!b.meets(22, 5, 35, 17))
            return;

        b.water(25, 0, 0, 32, 8, 20);
        for (int32_t i = 0; i < 4; ++i) {
            const int32_t z = 5 + i * 4;
            b.box(24, 2, z, 24, 4, z, k.mLight);
            b.box(22, 4, z, 23, 4, z, k.mLight);
            b.place(k.mLight, 25, 5, z);
            b.place(k.mLight, 26, 6, z);
            b.place(k.mLamp, 26, 5, z);
            b.box(33, 2, z, 33, 4, z, k.mLight);
            b.box(34, 4, z, 35, 4, z, k.mLight);
            b.place(k.mLight, 32, 5, z);
            b.place(k.mLight, 31, 6, z);
            b.place(k.mLamp, 31, 5, z);
            b.box(27, 6, z, 30, 6, z, k.mGray);
        }
    }

    void buildEntranceWall(Builder &b) {
        const Blocks &k = blocks();
        if (!b.meets(15, 20, 42, 21))
            return;

        b.box(15, 0, 21, 42, 0, 21, k.mGray);
        b.water(26, 1, 21, 31, 3, 21);
        b.box(21, 12, 21, 36, 12, 21, k.mGray);
        b.box(17, 11, 21, 40, 11, 21, k.mGray);
        b.box(16, 10, 21, 41, 10, 21, k.mGray);
        b.box(15, 7, 21, 42, 9, 21, k.mGray);
        b.box(16, 6, 21, 41, 6, 21, k.mGray);
        b.box(17, 5, 21, 40, 5, 21, k.mGray);
        b.box(21, 4, 21, 36, 4, 21, k.mGray);
        b.box(22, 3, 21, 26, 3, 21, k.mGray);
        b.box(31, 3, 21, 35, 3, 21, k.mGray);
        b.box(23, 2, 21, 25, 2, 21, k.mGray);
        b.box(32, 2, 21, 34, 2, 21, k.mGray);
        b.box(28, 4, 20, 29, 4, 21, k.mLight);
        b.place(k.mLight, 27, 3, 21);
        b.place(k.mLight, 30, 3, 21);
        b.place(k.mLight, 26, 2, 21);
        b.place(k.mLight, 31, 2, 21);
        b.place(k.mLight, 25, 1, 21);
        b.place(k.mLight, 32, 1, 21);
        for (int32_t i = 0; i < 7; ++i) {
            b.place(k.mBlack, 28 - i, 6 + i, 21);
            b.place(k.mBlack, 29 + i, 6 + i, 21);
        }
        for (int32_t i = 0; i < 4; ++i) {
            b.place(k.mBlack, 28 - i, 9 + i, 21);
            b.place(k.mBlack, 29 + i, 9 + i, 21);
        }
        b.place(k.mBlack, 28, 12, 21);
        b.place(k.mBlack, 29, 12, 21);
        for (int32_t i = 0; i < 3; ++i) {
            b.place(k.mBlack, 22 - i * 2, 8, 21);
            b.place(k.mBlack, 22 - i * 2, 9, 21);
            b.place(k.mBlack, 35 + i * 2, 8, 21);
            b.place(k.mBlack, 35 + i * 2, 9, 21);
        }
        b.water(15, 13, 21, 42, 15, 21);
        b.water(15, 1, 21, 15, 6, 21);
        b.water(16, 1, 21, 16, 5, 21);
        b.water(17, 1, 21, 20, 4, 21);
        b.water(21, 1, 21, 21, 3, 21);
        b.water(22, 1, 21, 22, 2, 21);
        b.water(23, 1, 21, 24, 1, 21);
        b.water(42, 1, 21, 42, 6, 21);
        b.water(41, 1, 21, 41, 5, 21);
        b.water(37, 1, 21, 40, 4, 21);
        b.water(36, 1, 21, 36, 3, 21);
        b.water(33, 1, 21, 34, 1, 21);
        b.water(35, 1, 21, 35, 2, 21);
    }

    void buildRoof(Builder &b) {
        const Blocks &k = blocks();
        if (!b.meets(21, 21, 36, 36))
            return;

        b.box(21, 0, 22, 36, 0, 36, k.mGray);
        b.water(21, 1, 22, 36, 23, 36);
        for (int32_t i = 0; i < 4; ++i) {
            b.box(21 + i, 13 + i, 21 + i, 36 - i, 13 + i, 21 + i, k.mLight);
            b.box(21 + i, 13 + i, 36 - i, 36 - i, 13 + i, 36 - i, k.mLight);
            b.box(21 + i, 13 + i, 22 + i, 21 + i, 13 + i, 35 - i, k.mLight);
            b.box(36 - i, 13 + i, 22 + i, 36 - i, 13 + i, 35 - i, k.mLight);
        }
        b.box(25, 16, 25, 32, 16, 32, k.mGray);
        b.box(25, 17, 25, 25, 19, 25, k.mLight);
        b.box(32, 17, 25, 32, 19, 25, k.mLight);
        b.box(25, 17, 32, 25, 19, 32, k.mLight);
        b.box(32, 17, 32, 32, 19, 32, k.mLight);
        b.place(k.mLight, 26, 20, 26);
        b.place(k.mLight, 27, 21, 27);
        b.place(k.mLamp, 27, 20, 27);
        b.place(k.mLight, 26, 20, 31);
        b.place(k.mLight, 27, 21, 30);
        b.place(k.mLamp, 27, 20, 30);
        b.place(k.mLight, 31, 20, 31);
        b.place(k.mLight, 30, 21, 30);
        b.place(k.mLamp, 30, 20, 30);
        b.place(k.mLight, 31, 20, 26);
        b.place(k.mLight, 30, 21, 27);
        b.place(k.mLamp, 30, 20, 27);
        b.box(28, 21, 27, 29, 21, 27, k.mGray);
        b.box(27, 21, 28, 27, 21, 29, k.mGray);
        b.box(28, 21, 30, 29, 21, 30, k.mGray);
        b.box(30, 21, 28, 30, 21, 29, k.mGray);
    }

    void buildLowerWall(Builder &b) {
        const Blocks &k = blocks();
        if (b.meets(0, 21, 6, 58)) {
            b.box(0, 0, 21, 6, 0, 57, k.mGray);
            b.water(0, 1, 21, 6, 7, 57);
            b.box(4, 4, 21, 6, 4, 53, k.mGray);
            for (int32_t i = 0; i < 4; ++i)
                b.box(i, i + 1, 21, i, i + 1, 57 - i, k.mLight);
            for (int32_t z = 23; z < 53; z += 3)
                b.place(k.mLight, 5, 5, z);
            b.place(k.mLight, 5, 5, 52);
            // the game lays the steps a second time
            for (int32_t i = 0; i < 4; ++i)
                b.box(i, i + 1, 21, i, i + 1, 57 - i, k.mLight);
            b.box(4, 1, 52, 6, 3, 52, k.mGray);
            b.box(5, 1, 51, 5, 3, 53, k.mGray);
        }
        if (b.meets(51, 21, 58, 58)) {
            b.box(51, 0, 21, 57, 0, 57, k.mGray);
            b.water(51, 1, 21, 57, 7, 57);
            b.box(51, 4, 21, 53, 4, 53, k.mGray);
            for (int32_t i = 0; i < 4; ++i)
                b.box(57 - i, i + 1, 21, 57 - i, i + 1, 57 - i, k.mLight);
            for (int32_t z = 23; z < 53; z += 3)
                b.place(k.mLight, 52, 5, z);
            b.place(k.mLight, 52, 5, 52);
            b.box(51, 1, 52, 53, 3, 52, k.mGray);
            b.box(52, 1, 51, 52, 3, 53, k.mGray);
        }
        if (b.meets(0, 51, 57, 57)) {
            b.box(7, 0, 51, 50, 0, 57, k.mGray);
            b.water(7, 1, 51, 50, 10, 57);
            for (int32_t i = 0; i < 4; ++i)
                b.box(i + 1, i + 1, 57 - i, 56 - i, i + 1, 57 - i, k.mLight);
        }
    }

    void buildMiddleWall(Builder &b) {
        const Blocks &k = blocks();
        if (b.meets(7, 21, 13, 54)) {
            b.box(7, 0, 21, 13, 0, 50, k.mGray);
            b.water(7, 1, 21, 13, 10, 50);
            b.box(11, 8, 21, 13, 8, 53, k.mGray);
            for (int32_t i = 0; i < 4; ++i)
                b.box(i + 7, i + 5, 21, i + 7, i + 5, 54, k.mLight);
            for (int32_t z = 21; z <= 45; z += 3)
                b.place(k.mLight, 12, 9, z);
        }
        if (b.meets(44, 21, 50, 54)) {
            b.box(44, 0, 21, 50, 0, 50, k.mGray);
            b.water(44, 1, 21, 50, 10, 50);
            b.box(44, 8, 21, 46, 8, 53, k.mGray);
            for (int32_t i = 0; i < 4; ++i)
                b.box(50 - i, i + 5, 21, 50 - i, i + 5, 54, k.mLight);
            for (int32_t z = 21; z <= 45; z += 3)
                b.place(k.mLight, 45, 9, z);
        }
        if (b.meets(8, 44, 49, 54)) {
            b.box(14, 0, 44, 43, 0, 50, k.mGray);
            b.water(14, 1, 44, 43, 10, 50);
            for (int32_t x = 12; x <= 45; x += 3) {
                b.place(k.mLight, x, 9, 45);
                b.place(k.mLight, x, 9, 52);
                if (x == 12 || x == 18 || x == 24 || x == 33 || x == 39 || x == 45) {
                    b.place(k.mLight, x, 9, 47);
                    b.place(k.mLight, x, 9, 50);
                    b.place(k.mLight, x, 10, 45);
                    b.place(k.mLight, x, 10, 46);
                    b.place(k.mLight, x, 10, 51);
                    b.place(k.mLight, x, 10, 52);
                    b.place(k.mLight, x, 11, 47);
                    b.place(k.mLight, x, 11, 50);
                    b.place(k.mLight, x, 12, 48);
                    b.place(k.mLight, x, 12, 49);
                }
            }
            for (int32_t i = 0; i < 3; ++i)
                b.box(8 + i, 5 + i, 54, 49 - i, 5 + i, 54, k.mGray);
            b.box(11, 8, 54, 46, 8, 54, k.mLight);
            b.box(14, 8, 44, 43, 8, 53, k.mGray);
        }
    }

    void buildUpperWall(Builder &b) {
        const Blocks &k = blocks();
        if (b.meets(14, 21, 20, 43)) {
            b.box(14, 0, 21, 20, 0, 43, k.mGray);
            b.water(14, 1, 22, 20, 14, 43);
            b.box(18, 12, 22, 20, 12, 39, k.mGray);
            b.box(18, 12, 21, 20, 12, 21, k.mLight);
            for (int32_t i = 0; i < 4; ++i)
                b.box(i + 14, i + 9, 21, i + 14, i + 9, 43 - i, k.mLight);
            for (int32_t z = 23; z <= 39; z += 3)
                b.place(k.mLight, 19, 13, z);
        }
        if (b.meets(37, 21, 43, 43)) {
            b.box(37, 0, 21, 43, 0, 43, k.mGray);
            b.water(37, 1, 22, 43, 14, 43);
            b.box(37, 12, 22, 39, 12, 39, k.mGray);
            b.box(37, 12, 21, 39, 12, 21, k.mLight);
            for (int32_t i = 0; i < 4; ++i)
                b.box(43 - i, i + 9, 21, 43 - i, i + 9, 43 - i, k.mLight);
            for (int32_t z = 23; z <= 39; z += 3)
                b.place(k.mLight, 38, 13, z);
        }
        if (b.meets(15, 37, 42, 43)) {
            b.box(21, 0, 37, 36, 0, 43, k.mGray);
            b.water(21, 1, 37, 36, 14, 43);
            b.box(21, 12, 37, 36, 12, 39, k.mGray);
            for (int32_t i = 0; i < 4; ++i)
                b.box(15 + i, i + 9, 43 - i, 42 - i, i + 9, 43 - i, k.mLight);
            for (int32_t x = 21; x <= 36; x += 3)
                b.place(k.mLight, x, 13, 38);
        }
    }

    void buildBuilding(Builder &b) {
        buildWing(b, false, 0);
        buildWing(b, true, 33);
        buildEntranceArches(b);
        buildEntranceWall(b);
        buildRoof(b);
        buildLowerWall(b);
        buildMiddleWall(b);
        buildUpperWall(b);

        // the legs: a ring of 4x4 pillars every 9 blocks, none under the entrance
        for (int32_t column = 0; column < 7; ++column) {
            int32_t row = 0;
            while (row < 7) {
                if (row == 0 && column == 3)
                    row = 6;
                for (int32_t x = 0; x < 4; ++x) {
                    for (int32_t z = 0; z < 4; ++z)
                        b.pillar(column * 9 + x, row * 9 + z);
                }
                row += column != 0 && column != 6 ? 6 : 1;
            }
        }

        for (int32_t i = 0; i < 5; ++i) {
            b.water(-1 - i, i * 2, -1 - i, -1 - i, 23, 58 + i);
            b.water(58 + i, i * 2, -1 - i, 58 + i, 23, 58 + i);
            b.water(-i, i * 2, -1 - i, 57 + i, 23, -1 - i);
            b.water(-i, i * 2, 58 + i, 57 + i, 23, 58 + i);
        }
    }

    void buildEntry(Builder &b, const Room &room) {
        const Blocks &k = blocks();
        b.box(0, 3, 0, 2, 3, 7, k.mLight);
        b.box(5, 3, 0, 7, 3, 7, k.mLight);
        b.box(0, 2, 0, 1, 2, 7, k.mLight);
        b.box(6, 2, 0, 7, 2, 7, k.mLight);
        b.box(0, 1, 0, 0, 1, 7, k.mLight);
        b.box(7, 1, 0, 7, 1, 7, k.mLight);
        b.box(0, 1, 7, 7, 3, 7, k.mLight);
        b.box(1, 1, 0, 2, 3, 0, k.mLight);
        b.box(5, 1, 0, 6, 3, 0, k.mLight);
        if (room.mOpenings[NORTH])
            b.water(3, 1, 7, 4, 2, 7);
        if (room.mOpenings[WEST])
            b.water(0, 1, 3, 1, 2, 4);
        if (room.mOpenings[EAST])
            b.water(6, 1, 3, 7, 2, 4);
    }

    // the outer ring of the simple top rooms and the third simple design
    void buildDarkRing(Builder &b) {
        const Blocks &k = blocks();
        b.box(0, 1, 0, 0, 1, 7, k.mLight);
        b.box(7, 1, 0, 7, 1, 7, k.mLight);
        b.box(1, 1, 0, 6, 1, 0, k.mLight);
        b.box(1, 1, 7, 6, 1, 7, k.mLight);
        b.box(0, 2, 0, 0, 2, 7, k.mBlack);
        b.box(7, 2, 0, 7, 2, 7, k.mBlack);
        b.box(1, 2, 0, 6, 2, 0, k.mBlack);
        b.box(1, 2, 7, 6, 2, 7, k.mBlack);
        b.box(0, 3, 0, 0, 3, 7, k.mLight);
        b.box(7, 3, 0, 7, 3, 7, k.mLight);
        b.box(1, 3, 0, 6, 3, 0, k.mLight);
        b.box(1, 3, 7, 6, 3, 7, k.mLight);
        b.box(0, 1, 3, 0, 2, 4, k.mBlack);
        b.box(7, 1, 3, 7, 2, 4, k.mBlack);
        b.box(3, 1, 0, 4, 2, 0, k.mBlack);
        b.box(3, 1, 7, 4, 2, 7, k.mBlack);
    }

    void buildSimple(Builder &b, const Room &room, int32_t design, std::mt19937 &random) {
        const Blocks &k = blocks();
        if (room.mIndex >= 25)
            b.floor(0, 0, room.mOpenings[DOWN]);
        if (!hasLink(room, UP))
            b.fillWater(1, 6, 4, 1, 6, k.mGray);

        const int32_t sides =
                room.mOpenings[NORTH] + room.mOpenings[SOUTH] + room.mOpenings[EAST] + room.mOpenings[WEST];
        const bool pillar = design != 0 && FeatureRandom::nextBoolean(random) && !room.mOpenings[DOWN]
                            && !room.mOpenings[UP] && sides >= 2;
        if (design == 0) {
            b.box(0, 1, 0, 2, 1, 2, k.mLight);
            b.box(0, 3, 0, 2, 3, 2, k.mLight);
            b.box(0, 2, 0, 0, 2, 2, k.mGray);
            b.box(1, 2, 0, 2, 2, 0, k.mGray);
            b.place(k.mLamp, 1, 2, 1);
            b.box(5, 1, 0, 7, 1, 2, k.mLight);
            b.box(5, 3, 0, 7, 3, 2, k.mLight);
            b.box(7, 2, 0, 7, 2, 2, k.mGray);
            b.box(5, 2, 0, 6, 2, 0, k.mGray);
            b.place(k.mLamp, 6, 2, 1);
            b.box(0, 1, 5, 2, 1, 7, k.mLight);
            b.box(0, 3, 5, 2, 3, 7, k.mLight);
            b.box(0, 2, 5, 0, 2, 7, k.mGray);
            b.box(1, 2, 7, 2, 2, 7, k.mGray);
            b.place(k.mLamp, 1, 2, 6);
            b.box(5, 1, 5, 7, 1, 7, k.mLight);
            b.box(5, 3, 5, 7, 3, 7, k.mLight);
            b.box(7, 2, 5, 7, 2, 7, k.mGray);
            b.box(5, 2, 7, 6, 2, 7, k.mGray);
            b.place(k.mLamp, 6, 2, 6);
            if (room.mOpenings[SOUTH]) {
                b.box(3, 3, 0, 4, 3, 0, k.mLight);
            } else {
                b.box(3, 3, 0, 4, 3, 1, k.mLight);
                b.box(3, 2, 0, 4, 2, 0, k.mGray);
                b.box(3, 1, 0, 4, 1, 1, k.mLight);
            }
            if (room.mOpenings[NORTH]) {
                b.box(3, 3, 7, 4, 3, 7, k.mLight);
            } else {
                b.box(3, 3, 6, 4, 3, 7, k.mLight);
                b.box(3, 2, 7, 4, 2, 7, k.mGray);
                b.box(3, 1, 6, 4, 1, 7, k.mLight);
            }
            if (room.mOpenings[WEST]) {
                b.box(0, 3, 3, 0, 3, 4, k.mLight);
            } else {
                b.box(0, 3, 3, 1, 3, 4, k.mLight);
                b.box(0, 2, 3, 0, 2, 4, k.mGray);
                b.box(0, 1, 3, 1, 1, 4, k.mLight);
            }
            if (room.mOpenings[EAST]) {
                b.box(7, 3, 3, 7, 3, 4, k.mLight);
            } else {
                b.box(6, 3, 3, 7, 3, 4, k.mLight);
                b.box(7, 2, 3, 7, 2, 4, k.mGray);
                b.box(6, 1, 3, 7, 1, 4, k.mLight);
            }
        } else if (design == 1) {
            b.box(2, 1, 2, 2, 3, 2, k.mLight);
            b.box(2, 1, 5, 2, 3, 5, k.mLight);
            b.box(5, 1, 5, 5, 3, 5, k.mLight);
            b.box(5, 1, 2, 5, 3, 2, k.mLight);
            b.place(k.mLamp, 2, 2, 2);
            b.place(k.mLamp, 2, 2, 5);
            b.place(k.mLamp, 5, 2, 5);
            b.place(k.mLamp, 5, 2, 2);
            b.box(0, 1, 0, 1, 3, 0, k.mLight);
            b.box(0, 1, 1, 0, 3, 1, k.mLight);
            b.box(0, 1, 7, 1, 3, 7, k.mLight);
            b.box(0, 1, 6, 0, 3, 6, k.mLight);
            b.box(6, 1, 7, 7, 3, 7, k.mLight);
            b.box(7, 1, 6, 7, 3, 6, k.mLight);
            b.box(6, 1, 0, 7, 3, 0, k.mLight);
            b.box(7, 1, 1, 7, 3, 1, k.mLight);
            b.place(k.mGray, 1, 2, 0);
            b.place(k.mGray, 0, 2, 1);
            b.place(k.mGray, 1, 2, 7);
            b.place(k.mGray, 0, 2, 6);
            b.place(k.mGray, 6, 2, 7);
            b.place(k.mGray, 7, 2, 6);
            b.place(k.mGray, 6, 2, 0);
            b.place(k.mGray, 7, 2, 1);
            if (!room.mOpenings[SOUTH]) {
                b.box(1, 3, 0, 6, 3, 0, k.mLight);
                b.box(1, 2, 0, 6, 2, 0, k.mGray);
                b.box(1, 1, 0, 6, 1, 0, k.mLight);
            }
            if (!room.mOpenings[NORTH]) {
                b.box(1, 3, 7, 6, 3, 7, k.mLight);
                b.box(1, 2, 7, 6, 2, 7, k.mGray);
                b.box(1, 1, 7, 6, 1, 7, k.mLight);
            }
            if (!room.mOpenings[WEST]) {
                b.box(0, 3, 1, 0, 3, 6, k.mLight);
                b.box(0, 2, 1, 0, 2, 6, k.mGray);
                b.box(0, 1, 1, 0, 1, 6, k.mLight);
            }
            if (!room.mOpenings[EAST]) {
                b.box(7, 3, 1, 7, 3, 6, k.mLight);
                b.box(7, 2, 1, 7, 2, 6, k.mGray);
                b.box(7, 1, 1, 7, 1, 6, k.mLight);
            }
        } else if (design == 2) {
            buildDarkRing(b);
            if (room.mOpenings[SOUTH])
                b.water(3, 1, 0, 4, 2, 0);
            if (room.mOpenings[NORTH])
                b.water(3, 1, 7, 4, 2, 7);
            if (room.mOpenings[WEST])
                b.water(0, 1, 3, 0, 2, 4);
            if (room.mOpenings[EAST])
                b.water(7, 1, 3, 7, 2, 4);
        }

        if (pillar) {
            b.box(3, 1, 3, 4, 1, 4, k.mLight);
            b.box(3, 2, 3, 4, 2, 4, k.mGray);
            b.box(3, 3, 3, 4, 3, 4, k.mLight);
        }
    }

    void buildSimpleTop(Builder &b, const Room &room, std::mt19937 &random) {
        const Blocks &k = blocks();
        if (room.mIndex >= 25)
            b.floor(0, 0, room.mOpenings[DOWN]);
        if (!hasLink(room, UP))
            b.fillWater(1, 6, 4, 1, 6, k.mGray);

        for (int32_t x = 1; x <= 6; ++x) {
            for (int32_t z = 1; z <= 6; ++z) {
                if (FeatureRandom::nextInt(random, 3) == 0)
                    continue;
                const int32_t bottom = FeatureRandom::nextInt(random, 4) == 0 ? 2 : 3;
                b.box(x, bottom, z, x, 3, z, k.mSponge);
            }
        }
        buildDarkRing(b);
        if (room.mOpenings[SOUTH])
            b.water(3, 1, 0, 4, 2, 0);
    }

    void buildDoubleY(Builder &b, const std::vector<Room> &rooms, const Room &room) {
        const Blocks &k = blocks();
        // the game reads the south opening here, not the one below
        if (room.mIndex >= 25)
            b.floor(0, 0, room.mOpenings[SOUTH]);
        const Room &up = linked(rooms, room, UP);
        if (!hasLink(up, UP))
            b.fillWater(1, 6, 8, 1, 6, k.mGray);

        b.box(0, 4, 0, 0, 4, 7, k.mLight);
        b.box(7, 4, 0, 7, 4, 7, k.mLight);
        b.box(1, 4, 0, 6, 4, 0, k.mLight);
        b.box(1, 4, 7, 6, 4, 7, k.mLight);
        b.box(2, 4, 1, 2, 4, 2, k.mLight);
        b.box(1, 4, 2, 1, 4, 2, k.mLight);
        b.box(5, 4, 1, 5, 4, 2, k.mLight);
        b.box(6, 4, 2, 6, 4, 2, k.mLight);
        b.box(2, 4, 5, 2, 4, 6, k.mLight);
        b.box(1, 4, 5, 1, 4, 5, k.mLight);
        b.box(5, 4, 5, 5, 4, 6, k.mLight);
        b.box(6, 4, 5, 6, 4, 5, k.mLight);

        const Room *floor = &room;
        for (int32_t y = 1; y <= 5; y += 4) {
            for (const int32_t z: {0, 7}) {
                if (floor->mOpenings[z == 0 ? SOUTH : NORTH]) {
                    b.box(2, y, z, 2, y + 2, z, k.mLight);
                    b.box(5, y, z, 5, y + 2, z, k.mLight);
                    b.box(3, y + 2, z, 4, y + 2, z, k.mLight);
                } else {
                    b.box(0, y, z, 7, y + 2, z, k.mLight);
                    b.box(0, y + 1, z, 7, y + 1, z, k.mGray);
                }
            }
            for (const int32_t x: {0, 7}) {
                if (floor->mOpenings[x == 0 ? WEST : EAST]) {
                    b.box(x, y, 2, x, y + 2, 2, k.mLight);
                    b.box(x, y, 5, x, y + 2, 5, k.mLight);
                    b.box(x, y + 2, 3, x, y + 2, 4, k.mLight);
                } else {
                    b.box(x, y, 0, x, y + 2, 7, k.mLight);
                    b.box(x, y + 1, 0, x, y + 1, 7, k.mGray);
                }
            }
            floor = &up;
        }
    }

    void buildDoubleX(Builder &b, const std::vector<Room> &rooms, const Room &room) {
        const Blocks &k = blocks();
        const Room &east = linked(rooms, room, EAST);
        if (room.mIndex >= 25) {
            b.floor(8, 0, east.mOpenings[DOWN]);
            b.floor(0, 0, room.mOpenings[DOWN]);
        }
        if (!hasLink(room, UP))
            b.fillWater(1, 7, 4, 1, 6, k.mGray);
        if (!hasLink(east, UP))
            b.fillWater(8, 14, 4, 1, 6, k.mGray);

        for (int32_t y = 3; y >= 1; --y) {
            const BlockState &state = y == 2 ? k.mGray : k.mLight;
            b.box(0, y, 0, 0, y, 7, state);
            b.box(15, y, 0, 15, y, 7, state);
            b.box(1, y, 0, 15, y, 0, state);
            b.box(1, y, 7, 14, y, 7, state);
        }
        b.box(5, 1, 0, 10, 1, 4, k.mLight);
        b.box(6, 2, 0, 9, 2, 3, k.mGray);
        b.box(5, 3, 0, 10, 3, 4, k.mLight);
        b.place(k.mLamp, 6, 2, 3);
        b.place(k.mLamp, 9, 2, 3);
        if (room.mOpenings[SOUTH])
            b.water(3, 1, 0, 4, 2, 0);
        if (room.mOpenings[NORTH])
            b.water(3, 1, 7, 4, 2, 7);
        if (room.mOpenings[WEST])
            b.water(0, 1, 3, 0, 2, 4);
        if (east.mOpenings[SOUTH])
            b.water(11, 1, 0, 12, 2, 0);
        if (east.mOpenings[NORTH])
            b.water(11, 1, 7, 12, 2, 7);
        if (east.mOpenings[EAST])
            b.water(15, 1, 3, 15, 2, 4);
    }

    void buildDoubleZ(Builder &b, const std::vector<Room> &rooms, const Room &room) {
        const Blocks &k = blocks();
        const Room &north = linked(rooms, room, NORTH);
        if (room.mIndex >= 25) {
            b.floor(0, 8, north.mOpenings[DOWN]);
            b.floor(0, 0, room.mOpenings[DOWN]);
        }
        if (!hasLink(room, UP))
            b.fillWater(1, 6, 4, 1, 7, k.mGray);
        if (!hasLink(north, UP))
            b.fillWater(1, 6, 4, 8, 14, k.mGray);

        for (int32_t y = 3; y >= 1; --y) {
            const BlockState &state = y == 2 ? k.mGray : k.mLight;
            b.box(0, y, 0, 0, y, 15, state);
            b.box(7, y, 0, 7, y, 15, state);
            b.box(1, y, 0, 7, y, 0, state);
            b.box(1, y, 15, 6, y, 15, state);
        }
        b.box(1, 1, 1, 1, 1, 2, k.mLight);
        b.box(6, 1, 1, 6, 1, 2, k.mLight);
        b.box(1, 3, 1, 1, 3, 2, k.mLight);
        b.box(6, 3, 1, 6, 3, 2, k.mLight);
        b.box(1, 1, 13, 1, 1, 14, k.mLight);
        b.box(6, 1, 13, 6, 1, 14, k.mLight);
        b.box(1, 3, 13, 1, 3, 14, k.mLight);
        b.box(6, 3, 13, 6, 3, 14, k.mLight);
        b.box(2, 1, 6, 2, 3, 6, k.mLight);
        b.box(5, 1, 6, 5, 3, 6, k.mLight);
        b.box(2, 1, 9, 2, 3, 9, k.mLight);
        b.box(5, 1, 9, 5, 3, 9, k.mLight);
        b.box(3, 2, 6, 4, 2, 6, k.mLight);
        b.box(3, 2, 9, 4, 2, 9, k.mLight);
        b.box(2, 2, 7, 2, 2, 8, k.mLight);
        b.box(5, 2, 7, 5, 2, 8, k.mLight);
        b.place(k.mLamp, 2, 2, 5);
        b.place(k.mLamp, 5, 2, 5);
        b.place(k.mLamp, 2, 2, 10);
        b.place(k.mLamp, 5, 2, 10);
        b.place(k.mLight, 2, 3, 5);
        b.place(k.mLight, 5, 3, 5);
        b.place(k.mLight, 2, 3, 10);
        b.place(k.mLight, 5, 3, 10);
        if (room.mOpenings[SOUTH])
            b.water(3, 1, 0, 4, 2, 0);
        if (room.mOpenings[EAST])
            b.water(7, 1, 3, 7, 2, 4);
        if (room.mOpenings[WEST])
            b.water(0, 1, 3, 0, 2, 4);
        if (north.mOpenings[NORTH])
            b.water(3, 1, 15, 4, 2, 15);
        if (north.mOpenings[WEST])
            b.water(0, 1, 11, 0, 2, 12);
        if (north.mOpenings[EAST])
            b.water(7, 1, 11, 7, 2, 12);
    }

    void buildDoubleXY(Builder &b, const std::vector<Room> &rooms, const Room &room) {
        const Blocks &k = blocks();
        const Room &east = linked(rooms, room, EAST);
        const Room &up = linked(rooms, room, UP);
        const Room &eastUp = linked(rooms, east, UP);
        if (room.mIndex >= 25) {
            b.floor(8, 0, east.mOpenings[DOWN]);
            b.floor(0, 0, room.mOpenings[DOWN]);
        }
        if (!hasLink(up, UP))
            b.fillWater(1, 7, 8, 1, 6, k.mGray);
        if (!hasLink(eastUp, UP))
            b.fillWater(8, 14, 8, 1, 6, k.mGray);

        for (int32_t y = 1; y <= 7; ++y) {
            const BlockState &state = (y & 3) == 2 ? k.mGray : k.mLight;
            b.box(0, y, 0, 0, y, 7, state);
            b.box(15, y, 0, 15, y, 7, state);
            b.box(1, y, 0, 15, y, 0, state);
            b.box(1, y, 7, 14, y, 7, state);
        }
        b.box(2, 1, 3, 2, 7, 4, k.mLight);
        b.box(3, 1, 2, 4, 7, 2, k.mLight);
        b.box(3, 1, 5, 4, 7, 5, k.mLight);
        b.box(13, 1, 3, 13, 7, 4, k.mLight);
        b.box(11, 1, 2, 12, 7, 2, k.mLight);
        b.box(11, 1, 5, 12, 7, 5, k.mLight);
        b.box(5, 1, 3, 5, 3, 4, k.mLight);
        b.box(10, 1, 3, 10, 3, 4, k.mLight);
        b.box(5, 7, 2, 10, 7, 5, k.mLight);
        b.box(5, 5, 2, 5, 7, 2, k.mLight);
        b.box(10, 5, 2, 10, 7, 2, k.mLight);
        b.box(5, 5, 5, 5, 7, 5, k.mLight);
        b.box(10, 5, 5, 10, 7, 5, k.mLight);
        b.place(k.mLight, 6, 6, 2);
        b.place(k.mLight, 9, 6, 2);
        b.place(k.mLight, 6, 6, 5);
        b.place(k.mLight, 9, 6, 5);
        b.box(5, 4, 3, 6, 4, 4, k.mLight);
        b.box(9, 4, 3, 10, 4, 4, k.mLight);
        b.place(k.mLamp, 5, 4, 2);
        b.place(k.mLamp, 5, 4, 5);
        b.place(k.mLamp, 10, 4, 2);
        b.place(k.mLamp, 10, 4, 5);
        if (room.mOpenings[SOUTH])
            b.water(3, 1, 0, 4, 2, 0);
        if (room.mOpenings[NORTH])
            b.water(3, 1, 7, 4, 2, 7);
        if (room.mOpenings[WEST])
            b.water(0, 1, 3, 0, 2, 4);
        if (east.mOpenings[SOUTH])
            b.water(11, 1, 0, 12, 2, 0);
        if (east.mOpenings[NORTH])
            b.water(11, 1, 7, 12, 2, 7);
        if (east.mOpenings[EAST])
            b.water(15, 1, 3, 15, 2, 4);
        if (up.mOpenings[SOUTH])
            b.water(3, 5, 0, 4, 6, 0);
        if (up.mOpenings[NORTH])
            b.water(3, 5, 7, 4, 6, 7);
        if (up.mOpenings[WEST])
            b.water(0, 5, 3, 0, 6, 4);
        if (eastUp.mOpenings[SOUTH])
            b.water(11, 5, 0, 12, 6, 0);
        if (eastUp.mOpenings[NORTH])
            b.water(11, 5, 7, 12, 6, 7);
        if (eastUp.mOpenings[EAST])
            b.water(15, 5, 3, 15, 6, 4);
    }

    void buildDoubleYZ(Builder &b, const std::vector<Room> &rooms, const Room &room) {
        const Blocks &k = blocks();
        const Room &north = linked(rooms, room, NORTH);
        const Room &up = linked(rooms, room, UP);
        const Room &northUp = linked(rooms, north, UP);
        if (room.mIndex >= 25) {
            b.floor(0, 8, north.mOpenings[DOWN]);
            b.floor(0, 0, room.mOpenings[DOWN]);
        }
        if (!hasLink(up, UP))
            b.fillWater(1, 6, 8, 1, 7, k.mGray);
        if (!hasLink(northUp, UP))
            b.fillWater(1, 6, 8, 8, 14, k.mGray);

        for (int32_t y = 1; y <= 7; ++y) {
            const BlockState &state = (y & 3) == 2 ? k.mGray : k.mLight;
            b.box(0, y, 0, 0, y, 15, state);
            b.box(7, y, 0, 7, y, 15, state);
            b.box(1, y, 0, 6, y, 0, state);
            b.box(1, y, 15, 6, y, 15, state);
        }
        for (int32_t y = 1; y <= 7; ++y)
            b.box(3, y, 7, 4, y, 8, (y & 3) == 2 ? k.mLamp : k.mBlack);

        if (room.mOpenings[SOUTH])
            b.water(3, 1, 0, 4, 2, 0);
        if (room.mOpenings[EAST])
            b.water(7, 1, 3, 7, 2, 4);
        if (room.mOpenings[WEST])
            b.water(0, 1, 3, 0, 2, 4);
        if (north.mOpenings[NORTH])
            b.water(3, 1, 15, 4, 2, 15);
        if (north.mOpenings[WEST])
            b.water(0, 1, 11, 0, 2, 12);
        if (north.mOpenings[EAST])
            b.water(7, 1, 11, 7, 2, 12);
        if (up.mOpenings[SOUTH])
            b.water(3, 5, 0, 4, 6, 0);
        if (up.mOpenings[EAST]) {
            b.water(7, 5, 3, 7, 6, 4);
            b.box(5, 4, 2, 6, 4, 5, k.mLight);
            b.box(6, 1, 2, 6, 3, 2, k.mLight);
            b.box(6, 1, 5, 6, 3, 5, k.mLight);
        }
        if (up.mOpenings[WEST]) {
            b.water(0, 5, 3, 0, 6, 4);
            b.box(1, 4, 2, 2, 4, 5, k.mLight);
            b.box(1, 1, 2, 1, 3, 2, k.mLight);
            b.box(1, 1, 5, 1, 3, 5, k.mLight);
        }
        if (northUp.mOpenings[NORTH])
            b.water(3, 5, 15, 4, 6, 15);
        if (northUp.mOpenings[WEST]) {
            b.water(0, 5, 11, 0, 6, 12);
            b.box(1, 4, 10, 2, 4, 13, k.mLight);
            b.box(1, 1, 10, 1, 3, 10, k.mLight);
            b.box(1, 1, 13, 1, 3, 13, k.mLight);
        }
        if (northUp.mOpenings[EAST]) {
            b.water(7, 5, 11, 7, 6, 12);
            b.box(5, 4, 10, 6, 4, 13, k.mLight);
            b.box(6, 1, 10, 6, 3, 10, k.mLight);
            b.box(6, 1, 13, 6, 3, 13, k.mLight);
        }
    }

    void buildCore(Builder &b) {
        const Blocks &k = blocks();
        b.fillWater(1, 14, 8, 0, 14, k.mGray);
        b.box(0, 7, 0, 0, 7, 15, k.mLight);
        b.box(15, 7, 0, 15, 7, 15, k.mLight);
        b.box(1, 7, 0, 15, 7, 0, k.mLight);
        b.box(1, 7, 15, 14, 7, 15, k.mLight);
        for (int32_t y = 1; y <= 6; ++y) {
            const BlockState &state = (y & 3) == 2 ? k.mGray : k.mLight;
            for (const int32_t x: {0, 15}) {
                b.box(x, y, 0, x, y, 1, state);
                b.box(x, y, 6, x, y, 9, state);
                b.box(x, y, 14, x, y, 15, state);
            }
            b.box(1, y, 0, 1, y, 0, state);
            b.box(6, y, 0, 9, y, 0, state);
            b.box(14, y, 0, 14, y, 0, state);
            b.box(1, y, 15, 14, y, 15, state);
        }
        b.box(6, 3, 6, 9, 6, 9, k.mBlack);
        b.box(7, 4, 7, 8, 5, 8, k.mGold);
        for (int32_t y = 3; y <= 6; y += 3) {
            for (int32_t x = 6; x <= 9; x += 3) {
                b.place(k.mLamp, x, y, 6);
                b.place(k.mLamp, x, y, 9);
            }
        }
        b.box(5, 1, 6, 5, 2, 6, k.mLight);
        b.box(5, 1, 9, 5, 2, 9, k.mLight);
        b.box(10, 1, 6, 10, 2, 6, k.mLight);
        b.box(10, 1, 9, 10, 2, 9, k.mLight);
        b.box(6, 1, 5, 6, 2, 5, k.mLight);
        b.box(9, 1, 5, 9, 2, 5, k.mLight);
        b.box(6, 1, 10, 6, 2, 10, k.mLight);
        b.box(9, 1, 10, 9, 2, 10, k.mLight);
        b.box(5, 2, 5, 5, 6, 5, k.mLight);
        b.box(5, 2, 10, 5, 6, 10, k.mLight);
        b.box(10, 2, 5, 10, 6, 5, k.mLight);
        b.box(10, 2, 10, 10, 6, 10, k.mLight);
        b.box(5, 7, 1, 5, 7, 6, k.mLight);
        b.box(10, 7, 1, 10, 7, 6, k.mLight);
        b.box(5, 7, 9, 5, 7, 14, k.mLight);
        b.box(10, 7, 9, 10, 7, 14, k.mLight);
        b.box(1, 7, 5, 6, 7, 5, k.mLight);
        b.box(1, 7, 10, 6, 7, 10, k.mLight);
        b.box(9, 7, 5, 14, 7, 5, k.mLight);
        b.box(9, 7, 10, 14, 7, 10, k.mLight);
        b.box(2, 1, 2, 2, 1, 3, k.mLight);
        b.box(3, 1, 2, 3, 1, 2, k.mLight);
        b.box(13, 1, 2, 13, 1, 3, k.mLight);
        b.box(12, 1, 2, 12, 1, 2, k.mLight);
        b.box(2, 1, 12, 2, 1, 13, k.mLight);
        b.box(3, 1, 13, 3, 1, 13, k.mLight);
        b.box(13, 1, 12, 13, 1, 13, k.mLight);
        b.box(12, 1, 13, 12, 1, 13, k.mLight);
    }

    void buildWingRoom(Builder &b, int32_t design) {
        const Blocks &k = blocks();
        if (design == 0) {
            for (int32_t i = 0; i < 4; ++i)
                b.box(10 - i, 3 - i, 20 - i, 12 + i, 3 - i, 20, k.mLight);
            b.box(7, 0, 6, 15, 0, 16, k.mLight);
            b.box(6, 0, 6, 6, 3, 20, k.mLight);
            b.box(16, 0, 6, 16, 3, 20, k.mLight);
            b.box(7, 1, 7, 7, 1, 20, k.mLight);
            b.box(15, 1, 7, 15, 1, 20, k.mLight);
            b.box(7, 1, 6, 9, 3, 6, k.mLight);
            b.box(13, 1, 6, 15, 3, 6, k.mLight);
            b.box(8, 1, 7, 9, 1, 7, k.mLight);
            b.box(13, 1, 7, 14, 1, 7, k.mLight);
            b.box(9, 0, 5, 13, 0, 5, k.mLight);
            b.box(10, 0, 7, 12, 0, 7, k.mBlack);
            b.box(8, 0, 10, 8, 0, 12, k.mBlack);
            b.box(14, 0, 10, 14, 0, 12, k.mBlack);
            for (int32_t z = 18; z >= 7; z -= 3) {
                b.place(k.mLamp, 6, 3, z);
                b.place(k.mLamp, 16, 3, z);
            }
            b.place(k.mLamp, 10, 0, 10);
            b.place(k.mLamp, 12, 0, 10);
            b.place(k.mLamp, 10, 0, 12);
            b.place(k.mLamp, 12, 0, 12);
            b.place(k.mLamp, 8, 3, 6);
            b.place(k.mLamp, 14, 3, 6);
            for (const int32_t z: {4, 18}) {
                for (const int32_t x: {4, 18}) {
                    b.place(k.mLight, x, 2, z);
                    b.place(k.mLamp, x, 1, z);
                    b.place(k.mLight, x, 0, z);
                }
            }
            b.place(k.mLight, 9, 7, 20);
            b.place(k.mLight, 13, 7, 20);
            b.box(6, 0, 21, 7, 4, 21, k.mLight);
            b.box(15, 0, 21, 16, 4, 21, k.mLight);
        } else if (design == 1) {
            b.box(9, 3, 18, 13, 3, 20, k.mLight);
            b.box(9, 0, 18, 9, 2, 18, k.mLight);
            b.box(13, 0, 18, 13, 2, 18, k.mLight);
            for (const int32_t x: {9, 13}) {
                b.place(k.mLight, x, 6, 20);
                b.place(k.mLamp, x, 5, 20);
                b.place(k.mLight, x, 4, 20);
            }
            b.box(7, 3, 7, 15, 3, 14, k.mLight);
            for (const int32_t x: {10, 12}) {
                b.box(x, 0, 10, x, 6, 10, k.mLight);
                b.box(x, 0, 12, x, 6, 12, k.mLight);
                b.place(k.mLamp, x, 0, 10);
                b.place(k.mLamp, x, 0, 12);
                b.place(k.mLamp, x, 4, 10);
                b.place(k.mLamp, x, 4, 12);
            }
            for (const int32_t x: {8, 14}) {
                b.box(x, 0, 7, x, 2, 7, k.mLight);
                b.box(x, 0, 14, x, 2, 14, k.mLight);
            }
            b.box(8, 3, 8, 8, 3, 13, k.mBlack);
            b.box(14, 3, 8, 14, 3, 13, k.mBlack);
        }
    }

    void buildPenthouse(Builder &b) {
        const Blocks &k = blocks();
        b.box(2, -1, 2, 11, -1, 11, k.mLight);
        b.box(0, -1, 0, 1, -1, 11, k.mGray);
        b.box(12, -1, 0, 13, -1, 11, k.mGray);
        b.box(2, -1, 0, 11, -1, 1, k.mGray);
        b.box(2, -1, 12, 11, -1, 13, k.mGray);
        b.box(0, 0, 0, 0, 0, 13, k.mLight);
        b.box(13, 0, 0, 13, 0, 13, k.mLight);
        b.box(1, 0, 0, 12, 0, 0, k.mLight);
        b.box(1, 0, 13, 12, 0, 13, k.mLight);
        for (int32_t i = 2; i <= 11; i += 3) {
            b.place(k.mLamp, 0, 0, i);
            b.place(k.mLamp, 13, 0, i);
            b.place(k.mLamp, i, 0, 0);
        }
        b.box(2, 0, 3, 4, 0, 9, k.mLight);
        b.box(9, 0, 3, 11, 0, 9, k.mLight);
        b.box(4, 0, 9, 9, 0, 11, k.mLight);
        b.place(k.mLight, 5, 0, 8);
        b.place(k.mLight, 8, 0, 8);
        b.place(k.mLight, 10, 0, 10);
        b.place(k.mLight, 3, 0, 10);
        b.box(3, 0, 3, 3, 0, 7, k.mBlack);
        b.box(10, 0, 3, 10, 0, 7, k.mBlack);
        b.box(6, 0, 10, 7, 0, 10, k.mBlack);
        for (int32_t z = 2; z <= 8; z += 3)
            b.box(3, 0, z, 3, 2, z, k.mLight);
        for (int32_t z = 2; z <= 8; z += 3)
            b.box(10, 0, z, 10, 2, z, k.mLight);
        b.box(5, 0, 10, 5, 2, 10, k.mLight);
        b.box(8, 0, 10, 8, 2, 10, k.mLight);
        b.box(6, -1, 7, 7, -1, 8, k.mBlack);
        b.water(6, -1, 3, 7, -1, 4);
    }
}

void MonumentPieces::postProcess(const Layout &layout, Level &level, LevelChunk &chunk, std::mt19937 &random,
                                 int32_t seaLevel) {
    if (layout.mPieces.empty() || !touchesChunk(layout.mPieces.front().getBox(), chunk))
        return;

    {
        Builder building(layout.mPieces.front(), level, chunk, seaLevel);
        buildBuilding(building);
    }

    const std::vector<Room> &rooms = layout.mRooms;
    for (size_t index = 1; index < layout.mPieces.size(); ++index) {
        const Piece &piece = layout.mPieces[index];
        if (!touchesChunk(piece.getBox(), chunk))
            continue;

        Builder b(piece, level, chunk, seaLevel);
        const Room *room = piece.mRoom == NO_ROOM ? nullptr : &rooms[(size_t) piece.mRoom];
        switch (piece.mType) {
            case Type::Entry:
                buildEntry(b, *room);
                break;
            case Type::Core:
                buildCore(b);
                break;
            case Type::Simple:
                buildSimple(b, *room, piece.mDesign, random);
                break;
            case Type::SimpleTop:
                buildSimpleTop(b, *room, random);
                break;
            case Type::DoubleX:
                buildDoubleX(b, rooms, *room);
                break;
            case Type::DoubleY:
                buildDoubleY(b, rooms, *room);
                break;
            case Type::DoubleZ:
                buildDoubleZ(b, rooms, *room);
                break;
            case Type::DoubleXY:
                buildDoubleXY(b, rooms, *room);
                break;
            case Type::DoubleYZ:
                buildDoubleYZ(b, rooms, *room);
                break;
            case Type::Wing:
                buildWingRoom(b, piece.mDesign);
                break;
            case Type::Penthouse:
                buildPenthouse(b);
                break;
            case Type::Building:
                break;
        }
    }
}
