#include "World/Level/LevelGen/Structure/StrongholdStructure.h"

#include "World/Level/LevelGen/Overworld/OverworldGeneratorConstants.h"
#include "World/Level/LevelGen/Random/GameRandom.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdlib>
#include <deque>
#include <map>
#include <mutex>
#include <random>
#include <tuple>
#include <utility>

namespace {
    using Type = StrongholdStructure::Type;
    using Door = StrongholdStructure::Door;
    using Piece = StrongholdStructure::Piece;
    using Box = StructurePiece::Box;

    const int32_t MAX_DEPTH = 50;
    const int32_t MAX_DISTANCE = 112;
    // a piece's floor stays above this height
    const int32_t LOWEST_FLOOR = 10;
    const int32_t TRIES = 5;
    const int32_t START_Y = 64;
    // the start is moved down until its top is at most this far under the sea
    const int32_t SEA_GAP = 10;
    // a filler corridor is never made this low
    const int32_t LOWEST_FILLER = 3;
    const size_t CACHED_PLANS = 16;

    struct Weight {
        Type mType;
        int32_t mWeight;
        int32_t mPlaced;
        int32_t mMax;
        int32_t mMinDepth;
    };

    std::vector<Weight> startWeights() {
        return {{Type::Straight, 40, 0, 0, 0},     {Type::PrisonHall, 5, 0, 5, 0},
                {Type::LeftTurn, 20, 0, 0, 0},     {Type::RightTurn, 20, 0, 0, 0},
                {Type::RoomCrossing, 10, 0, 6, 0}, {Type::StraightStairsDown, 5, 0, 5, 0},
                {Type::StairsDown, 5, 0, 5, 0},    {Type::FiveCrossing, 5, 0, 4, 0},
                {Type::ChestCorridor, 5, 0, 4, 0}, {Type::Library, 10, 0, 2, 5},
                {Type::PortalRoom, 10, 0, 1, 6}};
    }

    Door randomDoor(std::mt19937 &random) {
        const uint32_t value = (uint32_t) GameRandom::nextInt(random, 5);
        return value - 2 < 3 ? (Door) (value - 1) : Door::Opening;
    }

    bool nextBoolean(std::mt19937 &random) {
        return (GameRandom::nextUnsigned(random) & 0x8000000U) != 0;
    }

    void setXZ(Box &box, int32_t minX, int32_t maxX, int32_t minZ, int32_t maxZ) {
        box.mMinX = minX;
        box.mMaxX = maxX;
        box.mMinZ = minZ;
        box.mMaxZ = maxZ;
    }

    bool meetsXZ(const Box &box, int32_t minX, int32_t maxX, int32_t minZ, int32_t maxZ) {
        return box.mMaxX >= minX && box.mMinX <= maxX && box.mMaxZ >= minZ && box.mMinZ <= maxZ;
    }

    class Planner {
    public:
        explicit Planner(std::mt19937 &random) : mRandom(random), mWeights(startWeights()) {}

        void addChildren(size_t index) {
            const Piece piece = mPieces[index];
            const int32_t orientation = piece.getOrientation();
            switch (piece.mType) {
                case Type::Start:
                    mImposed = true;
                    forward(piece, 1);
                    break;
                case Type::StairsDown:
                case Type::ChestCorridor:
                case Type::PrisonHall:
                case Type::StraightStairsDown:
                    forward(piece, 1);
                    break;
                case Type::Straight:
                    forward(piece, 1);
                    if (piece.mFlags[0])
                        left(piece, 1, 2);
                    if (piece.mFlags[1])
                        right(piece, 1, 2);
                    break;
                case Type::LeftTurn:
                    if (orientation == StructurePiece::NORTH || orientation == StructurePiece::EAST)
                        left(piece, 1, 1);
                    else
                        right(piece, 1, 1);
                    break;
                case Type::RightTurn:
                    if (orientation == StructurePiece::NORTH || orientation == StructurePiece::EAST)
                        right(piece, 1, 1);
                    else
                        left(piece, 1, 1);
                    break;
                case Type::RoomCrossing:
                    forward(piece, 4);
                    left(piece, 1, 4);
                    right(piece, 1, 4);
                    break;
                case Type::FiveCrossing: {
                    // the low and high doors swap sides with the turn
                    const bool swapped = orientation == StructurePiece::WEST || orientation == StructurePiece::NORTH;
                    const int32_t low = swapped ? 5 : 3;
                    const int32_t high = swapped ? 3 : 5;
                    forward(piece, 5);
                    if (piece.mFlags[0])
                        left(piece, low, 1);
                    if (piece.mFlags[1])
                        left(piece, high, 7);
                    if (piece.mFlags[3])
                        right(piece, low, 1);
                    if (piece.mFlags[2])
                        right(piece, high, 7);
                    break;
                }
                case Type::PortalRoom:
                    mHasPortalRoom = true;
                    break;
                case Type::Library:
                case Type::FillerCorridor:
                    break;
            }
        }

        std::mt19937 &mRandom;
        std::vector<Piece> mPieces;
        std::vector<size_t> mPending;
        bool mHasPortalRoom = false;

    private:
        void forward(const Piece &piece, int32_t across) {
            const Box &box = piece.getBox();
            switch (piece.getOrientation()) {
                case StructurePiece::WEST:
                    generate(box.mMinX - 1, box.mMinY + 1, box.mMinZ + across, StructurePiece::WEST, piece.mDepth);
                    break;
                case StructurePiece::NORTH:
                    generate(box.mMinX + across, box.mMinY + 1, box.mMinZ - 1, StructurePiece::NORTH, piece.mDepth);
                    break;
                case StructurePiece::EAST:
                    generate(box.mMaxX + 1, box.mMinY + 1, box.mMinZ + across, StructurePiece::EAST, piece.mDepth);
                    break;
                default:
                    generate(box.mMinX + across, box.mMinY + 1, box.mMaxZ + 1, StructurePiece::SOUTH, piece.mDepth);
                    break;
            }
        }

        void left(const Piece &piece, int32_t up, int32_t across) {
            const Box &box = piece.getBox();
            if ((piece.getOrientation() & 1) == 0)
                generate(box.mMinX - 1, box.mMinY + up, box.mMinZ + across, StructurePiece::WEST, piece.mDepth);
            else
                generate(box.mMinX + across, box.mMinY + up, box.mMinZ - 1, StructurePiece::NORTH, piece.mDepth);
        }

        void right(const Piece &piece, int32_t up, int32_t across) {
            const Box &box = piece.getBox();
            if ((piece.getOrientation() & 1) == 0)
                generate(box.mMaxX + 1, box.mMinY + up, box.mMinZ + across, StructurePiece::EAST, piece.mDepth);
            else
                generate(box.mMinX + across, box.mMinY + up, box.mMaxZ + 1, StructurePiece::SOUTH, piece.mDepth);
        }

        bool fits(const Box &box) const {
            if (box.mMinY <= LOWEST_FLOOR)
                return false;
            for (const Piece &piece: mPieces) {
                if (piece.getBox().intersects(box))
                    return false;
            }
            return true;
        }

        // the piece's own draws come from the copy of the random the door was drawn with
        bool create(Type type, std::mt19937 &random, int32_t x, int32_t y, int32_t z, int32_t orientation,
                    int32_t depth, Piece &out) const {
            Piece piece(type, Box(), orientation, depth);
            Box &box = piece.getBox();
            switch (type) {
                case Type::Straight:
                    box = Box::orient(x, y, z, -1, -1, 0, 5, 5, 7, orientation);
                    if (!fits(box))
                        return false;
                    piece.mEntryDoor = randomDoor(random);
                    piece.mFlags[0] = GameRandom::nextInt(random, 2) == 0;
                    piece.mFlags[1] = GameRandom::nextInt(random, 2) == 0;
                    break;
                case Type::PrisonHall:
                    box = Box::orient(x, y, z, -1, -1, 0, 9, 5, 11, orientation);
                    if (!fits(box))
                        return false;
                    piece.mEntryDoor = randomDoor(random);
                    break;
                case Type::LeftTurn:
                case Type::RightTurn:
                    box = Box::orient(x, y, z, -1, -1, 0, 5, 5, 5, orientation);
                    if (!fits(box))
                        return false;
                    piece.mEntryDoor = randomDoor(random);
                    break;
                case Type::RoomCrossing:
                    box = Box::orient(x, y, z, -4, -1, 0, 11, 7, 11, orientation);
                    if (!fits(box))
                        return false;
                    piece.mEntryDoor = randomDoor(random);
                    piece.mValue = GameRandom::nextInt(random, 5);
                    break;
                case Type::StraightStairsDown:
                    box = Box::orient(x, y, z, -1, -7, 0, 5, 11, 8, orientation);
                    if (!fits(box))
                        return false;
                    piece.mEntryDoor = randomDoor(random);
                    break;
                case Type::StairsDown:
                    box = Box::orient(x, y, z, -1, -7, 0, 5, 11, 5, orientation);
                    if (!fits(box))
                        return false;
                    piece.mEntryDoor = randomDoor(random);
                    break;
                case Type::FiveCrossing:
                    box = Box::orient(x, y, z, -4, -3, 0, 10, 9, 11, orientation);
                    if (!fits(box))
                        return false;
                    piece.mEntryDoor = randomDoor(random);
                    piece.mFlags[0] = nextBoolean(random);
                    piece.mFlags[1] = nextBoolean(random);
                    piece.mFlags[3] = nextBoolean(random);
                    piece.mFlags[2] = GameRandom::nextInt(random, 3) > 0;
                    break;
                case Type::ChestCorridor:
                    box = Box::orient(x, y, z, -1, -1, 0, 5, 5, 7, orientation);
                    if (!fits(box))
                        return false;
                    piece.mEntryDoor = randomDoor(random);
                    break;
                case Type::Library:
                    box = Box::orient(x, y, z, -4, -1, 0, 14, 11, 15, orientation);
                    if (!fits(box)) {
                        box = Box::orient(x, y, z, -4, -1, 0, 14, 6, 15, orientation);
                        if (!fits(box))
                            return false;
                    }
                    piece.mEntryDoor = randomDoor(random);
                    piece.mFlags[0] = box.mMaxY - y + 2 >= 7;
                    break;
                case Type::PortalRoom:
                    box = Box::orient(x, y, z, -4, -1, 0, 11, 8, 16, orientation);
                    if (!fits(box))
                        return false;
                    break;
                case Type::Start:
                case Type::FillerCorridor:
                    return false;
            }
            out = piece;
            return true;
        }

        // the corridor runs up to the first piece in the way when that piece starts at the door's floor
        bool filler(std::mt19937 &random, int32_t x, int32_t y, int32_t z, int32_t orientation, int32_t depth,
                    Piece &out) const {
            Box probe;
            probe.mMinY = y - 1;
            probe.mMaxY = y + 3;
            switch (orientation) {
                case StructurePiece::WEST:
                    setXZ(probe, x - 3, x, z - 1, z + 3);
                    break;
                case StructurePiece::NORTH:
                    setXZ(probe, x - 1, x + 3, z - 3, z);
                    break;
                case StructurePiece::EAST:
                    setXZ(probe, x, x + 3, z - 1, z + 3);
                    break;
                default:
                    setXZ(probe, x - 1, x + 3, z, z + 3);
                    break;
            }

            const Piece *blocking = nullptr;
            for (const Piece &piece: mPieces) {
                if (piece.getBox().intersects(probe)) {
                    blocking = &piece;
                    break;
                }
            }
            if (blocking == nullptr || blocking->getBox().mMinY != y - 1)
                return false;

            // lengths 3, 2 then 1: the last test box is empty and meets only a piece across the door's plane
            const bool alongZ = (orientation & 1) == 0;
            int32_t length = 0;
            for (int32_t candidate = 3; candidate >= 1 && length == 0; --candidate) {
                const int32_t reach = candidate - 1;
                bool meets = false;
                switch (orientation) {
                    case StructurePiece::WEST:
                        meets = meetsXZ(blocking->getBox(), x - reach + 1, x, z - 1, z + 3);
                        break;
                    case StructurePiece::NORTH:
                        meets = meetsXZ(blocking->getBox(), x - 1, x + 3, z - reach + 1, z);
                        break;
                    case StructurePiece::EAST:
                        meets = meetsXZ(blocking->getBox(), x, x + reach - 1, z - 1, z + 3);
                        break;
                    default:
                        meets = meetsXZ(blocking->getBox(), x - 1, x + 3, z, z + reach - 1);
                        break;
                }
                if (!meets)
                    length = candidate;
            }
            if (length == 0)
                return false;

            Piece piece(Type::FillerCorridor, Box(), orientation, depth);
            Box &box = piece.getBox();
            box.mMinY = y - 1;
            box.mMaxY = y + 3;
            switch (orientation) {
                case StructurePiece::WEST:
                    setXZ(box, x - length + 1, x, z - 1, z + 3);
                    break;
                case StructurePiece::NORTH:
                    setXZ(box, x - 1, x + 3, z - length + 1, z);
                    break;
                case StructurePiece::EAST:
                    setXZ(box, x, x + length - 1, z - 1, z + 3);
                    break;
                default:
                    setXZ(box, x - 1, x + 3, z, z + length - 1);
                    break;
            }
            if (box.mMaxX < box.mMinX || y < LOWEST_FILLER || box.mMaxZ < box.mMinZ)
                return false;

            piece.mEntryDoor = randomDoor(random);
            piece.mValue = alongZ ? box.mMaxZ - box.mMinZ + 1 : box.mMaxX - box.mMinX + 1;
            out = piece;
            return true;
        }

        void generate(int32_t x, int32_t y, int32_t z, int32_t orientation, int32_t depth) {
            if (depth > MAX_DEPTH)
                return;
            const Box &start = mPieces.front().getBox();
            if (std::abs(x - start.mMinX) > MAX_DISTANCE || std::abs(z - start.mMinZ) > MAX_DISTANCE)
                return;

            const int32_t childDepth = depth + 1;
            std::mt19937 random = mRandom;
            Piece made;
            bool placed = false;
            if (mImposed) {
                placed = create(Type::FiveCrossing, random, x, y, z, orientation, childDepth, made);
                mImposed = false;
            }

            if (!placed) {
                int32_t total = 0;
                bool open = false;
                for (const Weight &weight: mWeights) {
                    open = open || (weight.mMax > 0 && weight.mPlaced < weight.mMax);
                    total += weight.mWeight;
                }
                if (total <= 0 || !open)
                    return;

                for (int32_t attempt = 0; attempt < TRIES && !placed; ++attempt) {
                    int32_t remaining = (int32_t) (GameRandom::nextUnsigned(random) % (uint32_t) total);
                    for (size_t i = 0; i < mWeights.size(); ++i) {
                        Weight &weight = mWeights[i];
                        remaining -= weight.mWeight;
                        if (remaining >= 0)
                            continue;
                        if ((weight.mMax != 0 && weight.mPlaced >= weight.mMax) || childDepth < weight.mMinDepth)
                            break;
                        if (mHasPrevious && weight.mType == mPrevious && mWeights.size() > 1)
                            break;
                        if (!create(weight.mType, random, x, y, z, orientation, childDepth, made))
                            continue;

                        ++weight.mPlaced;
                        mPrevious = weight.mType;
                        mHasPrevious = true;
                        if (weight.mMax != 0 && weight.mPlaced >= weight.mMax)
                            mWeights.erase(mWeights.begin() + (std::ptrdiff_t) i);
                        placed = true;
                        break;
                    }
                }

                if (!placed && !filler(random, x, y, z, orientation, childDepth, made))
                    return;
            }

            mPieces.push_back(made);
            mPending.push_back(mPieces.size() - 1);
        }

        std::vector<Weight> mWeights;
        bool mImposed = false;
        bool mHasPrevious = false;
        Type mPrevious = Type::Start;
    };

    // the last piece gives way to a portal room past the one before it, turned as it was
    void addMissingPortalRoom(std::vector<Piece> &pieces) {
        if (pieces.size() < 2)
            return;

        const int32_t orientation = pieces.back().getOrientation();
        pieces.pop_back();
        const Piece &previous = pieces.back();
        const Box &from = previous.getBox();
        Box box;
        switch (orientation) {
            case StructurePiece::WEST: {
                const int32_t middle = (from.mMinZ + from.mMaxZ) / 2;
                setXZ(box, from.mMinX - 15, from.mMinX, middle - 5, middle + 5);
                break;
            }
            case StructurePiece::NORTH: {
                const int32_t middle = (from.mMinX + from.mMaxX) / 2;
                setXZ(box, middle - 5, middle + 5, from.mMinZ - 15, from.mMinZ);
                break;
            }
            case StructurePiece::EAST: {
                const int32_t middle = (from.mMinZ + from.mMaxZ) / 2;
                setXZ(box, from.mMaxX, from.mMaxX + 15, middle - 3, middle + 7);
                break;
            }
            default: {
                const int32_t middle = (from.mMinX + from.mMaxX) / 2;
                setXZ(box, middle - 3, middle + 7, from.mMaxZ, from.mMaxZ + 15);
                break;
            }
        }
        if (from.mMinY <= LOWEST_FLOOR)
            return;

        box.mMinY = from.mMinY;
        box.mMaxY = from.mMinY + 7;
        pieces.emplace_back(Type::PortalRoom, box, orientation, previous.mDepth);
    }
}

StrongholdStructure::Layout StrongholdStructure::plan(int64_t seed, int32_t chunkX, int32_t chunkZ) {
    // the start goes on with the chunk's creation random, its first draw spent
    std::mt19937 random(StructureFeature::getPostProcessSeed(seed, chunkX, chunkZ));
    random();

    const int32_t orientation = GameRandom::nextInt(random, 4);
    Box stairs;
    stairs.mMinX = chunkX * 16 + 2;
    stairs.mMinY = START_Y;
    stairs.mMinZ = chunkZ * 16 + 2;
    stairs.mMaxX = chunkX * 16 + 6;
    stairs.mMaxY = START_Y + 10;
    stairs.mMaxZ = chunkZ * 16 + 6;
    const Piece start(Type::Start, stairs, orientation, 0);

    Planner planner(random);
    planner.mPieces.push_back(start);
    planner.addChildren(0);
    while (!planner.mPending.empty()) {
        const size_t pick = (size_t) GameRandom::nextInt(random, (int32_t) planner.mPending.size());
        const size_t index = planner.mPending[pick];
        planner.mPending.erase(planner.mPending.begin() + (std::ptrdiff_t) pick);
        planner.addChildren(index);
    }
    if (!planner.mHasPortalRoom)
        addMissingPortalRoom(planner.mPieces);

    Layout layout;
    layout.mPieces = std::move(planner.mPieces);
    Box &box = layout.mBox;
    box = {INT_MAX, INT_MAX, INT_MAX, INT_MIN, INT_MIN, INT_MIN};
    for (const Piece &piece: layout.mPieces) {
        box.mMinX = std::min(box.mMinX, piece.getBox().mMinX);
        box.mMinY = std::min(box.mMinY, piece.getBox().mMinY);
        box.mMinZ = std::min(box.mMinZ, piece.getBox().mMinZ);
        box.mMaxX = std::max(box.mMaxX, piece.getBox().mMaxX);
        box.mMaxY = std::max(box.mMaxY, piece.getBox().mMaxY);
        box.mMaxZ = std::max(box.mMaxZ, piece.getBox().mMaxZ);
    }

    // the game reads the sea level and the bottom of the dimension it generates in
    const int32_t highest = OverworldGeneratorConstants::SEA_LEVEL - SEA_GAP;
    int32_t top = box.mMaxY - box.mMinY + OverworldGeneratorConstants::MIN_Y + 2;
    if (highest > top)
        top += (int32_t) (GameRandom::nextUnsigned(random) % (uint32_t) (highest - top));
    const int32_t shift = top - box.mMaxY;
    box.mMinY += shift;
    box.mMaxY = top;
    for (Piece &piece: layout.mPieces)
        piece.getBox().move(0, shift, 0);
    return layout;
}

std::shared_ptr<const StrongholdStructure::Layout> StrongholdStructure::cachedPlan(int64_t seed, int32_t chunkX,
                                                                                    int32_t chunkZ) {
    using Key = std::tuple<int64_t, int32_t, int32_t>;
    static std::mutex mutex;
    static std::map<Key, std::shared_ptr<const Layout>> plans;
    static std::deque<Key> order;

    const Key key = {seed, chunkX, chunkZ};
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = plans.find(key);
        if (found != plans.end())
            return found->second;
    }

    // planned unlocked: other workers keep planning
    auto layout = std::make_shared<const Layout>(plan(seed, chunkX, chunkZ));
    std::lock_guard<std::mutex> lock(mutex);
    const auto inserted = plans.emplace(key, layout);
    if (!inserted.second)
        return inserted.first->second;

    order.push_back(key);
    if (order.size() > CACHED_PLANS) {
        plans.erase(order.front());
        order.pop_front();
    }
    return layout;
}

const char *StrongholdStructure::nameOf(Type type) {
    switch (type) {
        case Type::Start:
            return "Start";
        case Type::Straight:
            return "Straight";
        case Type::PrisonHall:
            return "PrisonHall";
        case Type::LeftTurn:
            return "LeftTurn";
        case Type::RightTurn:
            return "RightTurn";
        case Type::RoomCrossing:
            return "RoomCrossing";
        case Type::StraightStairsDown:
            return "StraightStairsDown";
        case Type::StairsDown:
            return "StairsDown";
        case Type::FiveCrossing:
            return "FiveCrossing";
        case Type::ChestCorridor:
            return "ChestCorridor";
        case Type::Library:
            return "Library";
        case Type::PortalRoom:
            return "PortalRoom";
        case Type::FillerCorridor:
            return "FillerCorridor";
    }
    return "";
}
