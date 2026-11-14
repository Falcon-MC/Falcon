#include "World/Level/LevelGen/Structure/NetherFortressPieces.h"

#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"

#include <algorithm>
#include <cstdlib>
#include <limits>

namespace {
    using Piece = NetherFortressPieces::Piece;
    using Type = NetherFortressPieces::Type;
    using Box = StructurePiece::Box;

    struct Weight {
        Type mType;
        int32_t mWeight;
        int32_t mMaxPlaceCount;
        bool mAllowInRow;
        int32_t mPlaceCount = 0;
    };

    // the start's two lists, in the order it fills them; a cap of 0 has no cap
    const Weight BRIDGE_WEIGHTS[] = {
            {Type::BridgeStraight, 30, 0, true}, {Type::BridgeCrossing, 10, 4, false},
            {Type::RoomCrossing, 10, 4, false},  {Type::StairsRoom, 10, 3, false},
            {Type::MonsterThrone, 5, 2, false},  {Type::CastleEntrance, 5, 1, false}};

    const Weight CASTLE_WEIGHTS[] = {{Type::CastleSmallCorridor, 25, 0, true},
                                     {Type::CastleSmallCorridorCrossing, 15, 5, false},
                                     {Type::CastleSmallCorridorRightTurn, 5, 10, false},
                                     {Type::CastleSmallCorridorLeftTurn, 5, 10, false},
                                     {Type::CastleCorridorStairs, 10, 3, true},
                                     {Type::CastleCorridorTBalcony, 7, 2, false},
                                     {Type::CastleStalkRoom, 5, 2, false}};

    // a box reaching y 10 or lower is refused
    const int32_t MIN_PIECE_Y = 11;

    const int32_t START_OFFSET = 2;
    const int32_t START_SIZE = 19;
    const int32_t START_MIN_Y = 64;
    const int32_t START_MAX_Y = 73;

    // the fortress floor is moved between the sea level plus 16 and plus 38
    const int32_t LOWEST_ABOVE_SEA = 16;
    const int32_t HIGHEST_ABOVE_SEA = 38;

    class Assembler {
    public:
        explicit Assembler(std::mt19937 &random)
                : mRandom(random), mBridges(std::begin(BRIDGE_WEIGHTS), std::end(BRIDGE_WEIGHTS)),
                  mCastle(std::begin(CASTLE_WEIGHTS), std::end(CASTLE_WEIGHTS)) {}

        std::vector<Piece> run(int32_t chunkX, int32_t chunkZ, int32_t seaLevel) {
            const int32_t orientation = FeatureRandom::nextInt(mRandom, 4);
            Box box;
            box.mMinX = chunkX * 16 + START_OFFSET;
            box.mMinY = START_MIN_Y;
            box.mMinZ = chunkZ * 16 + START_OFFSET;
            box.mMaxX = box.mMinX + START_SIZE - 1;
            box.mMaxY = START_MAX_Y;
            box.mMaxZ = box.mMinZ + START_SIZE - 1;
            mStartMinX = box.mMinX;
            mStartMinZ = box.mMinZ;
            mPieces.emplace_back(Type::Start, box, orientation, 0);

            _addChildren(0);
            while (!mPending.empty()) {
                const size_t index = (size_t) FeatureRandom::nextInt(mRandom, (int32_t) mPending.size());
                const size_t piece = mPending[index];
                mPending.erase(mPending.begin() + (std::ptrdiff_t) index);
                _addChildren(piece);
            }

            _moveToHeight(seaLevel);
            return std::move(mPieces);
        }

    private:
        bool _collides(const Box &box) const {
            for (const Piece &piece: mPieces) {
                if (piece.getBox().intersects(box))
                    return true;
            }
            return false;
        }

        static Box _boxOf(Type type, int32_t x, int32_t y, int32_t z, int32_t orientation) {
            switch (type) {
                case Type::BridgeStraight:
                    return Box::orient(x, y, z, -1, -3, 0, 5, 10, 19, orientation);
                case Type::BridgeCrossing:
                    return Box::orient(x, y, z, -8, -3, 0, 19, 10, 19, orientation);
                case Type::RoomCrossing:
                    return Box::orient(x, y, z, -2, 0, 0, 7, 9, 7, orientation);
                case Type::StairsRoom:
                    return Box::orient(x, y, z, -2, 0, 0, 7, 11, 7, orientation);
                case Type::MonsterThrone:
                    return Box::orient(x, y, z, -2, 0, 0, 7, 8, 9, orientation);
                case Type::CastleEntrance:
                case Type::CastleStalkRoom:
                    return Box::orient(x, y, z, -5, -3, 0, 13, 14, 13, orientation);
                case Type::CastleCorridorStairs:
                    return Box::orient(x, y, z, -1, -7, 0, 5, 14, 10, orientation);
                case Type::CastleCorridorTBalcony:
                    return Box::orient(x, y, z, -3, 0, 0, 9, 7, 9, orientation);
                case Type::BridgeEndFiller:
                    return Box::orient(x, y, z, -1, -3, 0, 5, 10, 8, orientation);
                default:
                    return Box::orient(x, y, z, -1, 0, 0, 5, 7, 5, orientation);
            }
        }

        bool _create(Type type, int32_t x, int32_t y, int32_t z, int32_t orientation, int32_t depth, Piece &out) {
            const Box box = _boxOf(type, x, y, z, orientation);
            if (box.mMinY < MIN_PIECE_Y || _collides(box))
                return false;

            out = Piece(type, box, orientation, depth);
            if (type == Type::CastleSmallCorridorRightTurn || type == Type::CastleSmallCorridorLeftTurn)
                out.mNeedsChest = FeatureRandom::nextInt(mRandom, 3) == 0;
            else if (type == Type::BridgeEndFiller)
                out.mSelfSeed = FeatureRandom::nextInt(mRandom);
            return true;
        }

        void _add(const Piece &piece) {
            mPieces.push_back(piece);
            mPending.push_back(mPieces.size() - 1);
        }

        void _endFiller(int32_t x, int32_t y, int32_t z, int32_t orientation, int32_t depth) {
            Piece piece;
            if (_create(Type::BridgeEndFiller, x, y, z, orientation, depth, piece))
                _add(piece);
        }

        void _generate(int32_t x, int32_t y, int32_t z, int32_t orientation, int32_t depth, bool castle) {
            if (std::abs(x - mStartMinX) > NetherFortressPieces::MAX_DISTANCE
                || std::abs(z - mStartMinZ) > NetherFortressPieces::MAX_DISTANCE) {
                // too far out: an end filler at the parent's own depth closes the bridge
                _endFiller(x, y, z, orientation, depth);
                return;
            }

            std::vector<Weight> &weights = castle ? mCastle : mBridges;
            const int32_t childDepth = depth + 1;
            bool open = false;
            int32_t total = 0;
            for (const Weight &weight: weights) {
                open = open || (weight.mMaxPlaceCount > 0 && weight.mPlaceCount < weight.mMaxPlaceCount);
                total += weight.mWeight;
            }

            if (depth < NetherFortressPieces::MAX_DEPTH && open && total > 0) {
                for (int32_t attempt = 0; attempt < 5; ++attempt) {
                    int32_t roll = FeatureRandom::nextInt(mRandom, total);
                    for (size_t index = 0; index < weights.size(); ++index) {
                        Weight &weight = weights[index];
                        roll -= weight.mWeight;
                        if (roll >= 0)
                            continue;

                        if (weight.mMaxPlaceCount != 0 && weight.mPlaceCount >= weight.mMaxPlaceCount)
                            break;
                        if (mHasPrevious && mPrevious == weight.mType && !weight.mAllowInRow)
                            break;

                        // a piece that does not fit leaves the roll running into the next weights
                        Piece piece;
                        if (!_create(weight.mType, x, y, z, orientation, childDepth, piece))
                            continue;

                        ++weight.mPlaceCount;
                        mPrevious = weight.mType;
                        mHasPrevious = true;
                        if (weight.mMaxPlaceCount != 0 && weight.mPlaceCount >= weight.mMaxPlaceCount)
                            weights.erase(weights.begin() + (std::ptrdiff_t) index);
                        _add(piece);
                        return;
                    }
                }
            }

            _endFiller(x, y, z, orientation, childDepth);
        }

        void _forward(const Piece &piece, int32_t offsetX, int32_t offsetY, bool castle) {
            const Box box = piece.getBox();
            const int32_t y = box.mMinY + offsetY;
            const int32_t depth = piece.mDepth;
            switch (piece.getOrientation()) {
                case StructurePiece::SOUTH:
                    _generate(box.mMinX + offsetX, y, box.mMaxZ + 1, StructurePiece::SOUTH, depth, castle);
                    break;
                case StructurePiece::WEST:
                    _generate(box.mMinX - 1, y, box.mMinZ + offsetX, StructurePiece::WEST, depth, castle);
                    break;
                case StructurePiece::NORTH:
                    _generate(box.mMinX + offsetX, y, box.mMinZ - 1, StructurePiece::NORTH, depth, castle);
                    break;
                case StructurePiece::EAST:
                    _generate(box.mMaxX + 1, y, box.mMinZ + offsetX, StructurePiece::EAST, depth, castle);
                    break;
                default:
                    break;
            }
        }

        void _left(const Piece &piece, int32_t offsetY, int32_t offsetX, bool castle) {
            const Box box = piece.getBox();
            const int32_t y = box.mMinY + offsetY;
            const int32_t depth = piece.mDepth;
            switch (piece.getOrientation()) {
                case StructurePiece::SOUTH:
                case StructurePiece::NORTH:
                    _generate(box.mMinX - 1, y, box.mMinZ + offsetX, StructurePiece::WEST, depth, castle);
                    break;
                case StructurePiece::WEST:
                case StructurePiece::EAST:
                    _generate(box.mMinX + offsetX, y, box.mMinZ - 1, StructurePiece::NORTH, depth, castle);
                    break;
                default:
                    break;
            }
        }

        void _right(const Piece &piece, int32_t offsetY, int32_t offsetX, bool castle) {
            const Box box = piece.getBox();
            const int32_t y = box.mMinY + offsetY;
            const int32_t depth = piece.mDepth;
            switch (piece.getOrientation()) {
                case StructurePiece::SOUTH:
                case StructurePiece::NORTH:
                    _generate(box.mMaxX + 1, y, box.mMinZ + offsetX, StructurePiece::EAST, depth, castle);
                    break;
                case StructurePiece::WEST:
                case StructurePiece::EAST:
                    _generate(box.mMinX + offsetX, y, box.mMaxZ + 1, StructurePiece::SOUTH, depth, castle);
                    break;
                default:
                    break;
            }
        }

        // the children open from a copy: adding pieces moves the vector
        void _addChildren(size_t index) {
            const Piece piece = mPieces[index];
            switch (piece.mType) {
                case Type::Start:
                case Type::BridgeCrossing:
                    _forward(piece, 8, 3, false);
                    _left(piece, 3, 8, false);
                    _right(piece, 3, 8, false);
                    break;
                case Type::BridgeStraight:
                    _forward(piece, 1, 3, false);
                    break;
                case Type::RoomCrossing:
                    _forward(piece, 2, 0, false);
                    _left(piece, 0, 2, false);
                    _right(piece, 0, 2, false);
                    break;
                case Type::StairsRoom:
                    _right(piece, 6, 2, false);
                    break;
                case Type::CastleEntrance:
                    _forward(piece, 5, 3, true);
                    break;
                case Type::CastleStalkRoom:
                    _forward(piece, 5, 3, true);
                    _forward(piece, 5, 11, true);
                    break;
                case Type::CastleSmallCorridor:
                case Type::CastleCorridorStairs:
                    _forward(piece, 1, 0, true);
                    break;
                case Type::CastleSmallCorridorCrossing:
                    _forward(piece, 1, 0, true);
                    _left(piece, 0, 1, true);
                    _right(piece, 0, 1, true);
                    break;
                case Type::CastleSmallCorridorRightTurn:
                    _right(piece, 0, 1, true);
                    break;
                case Type::CastleSmallCorridorLeftTurn:
                    _left(piece, 0, 1, true);
                    break;
                case Type::CastleCorridorTBalcony: {
                    const int32_t orientation = piece.getOrientation();
                    const int32_t offset =
                            orientation == StructurePiece::WEST || orientation == StructurePiece::NORTH ? 5 : 1;
                    const bool leftCastle = FeatureRandom::nextInt(mRandom, 8) > 0;
                    _left(piece, 0, offset, leftCastle);
                    const bool rightCastle = FeatureRandom::nextInt(mRandom, 8) > 0;
                    _right(piece, 0, offset, rightCastle);
                    break;
                }
                default:
                    break;
            }
        }

        void _moveToHeight(int32_t seaLevel) {
            int32_t minY = std::numeric_limits<int32_t>::max();
            int32_t maxY = std::numeric_limits<int32_t>::min() + 1;
            for (const Piece &piece: mPieces) {
                minY = std::min(minY, piece.getBox().mMinY);
                maxY = std::max(maxY, piece.getBox().mMaxY);
            }

            const int32_t lowest = seaLevel + LOWEST_ABOVE_SEA;
            const int32_t range = seaLevel + HIGHEST_ABOVE_SEA - lowest + minY - maxY;
            int32_t floor = lowest;
            if (range >= 2)
                floor += FeatureRandom::nextInt(mRandom, range);

            const int32_t shift = floor - minY;
            for (Piece &piece: mPieces)
                piece.getBox().move(0, shift, 0);
        }

        std::mt19937 &mRandom;
        std::vector<Piece> mPieces;
        std::vector<size_t> mPending;
        std::vector<Weight> mBridges;
        std::vector<Weight> mCastle;
        bool mHasPrevious = false;
        Type mPrevious = Type::Start;
        int32_t mStartMinX = 0;
        int32_t mStartMinZ = 0;
    };
}

std::vector<NetherFortressPieces::Piece> NetherFortressPieces::assemble(std::mt19937 &random, int32_t chunkX,
                                                                       int32_t chunkZ, int32_t seaLevel) {
    return Assembler(random).run(chunkX, chunkZ, seaLevel);
}

const char *NetherFortressPieces::getName(Type type) {
    switch (type) {
        case Type::Start:
            return "StartPiece";
        case Type::BridgeStraight:
            return "BridgeStraight";
        case Type::BridgeCrossing:
            return "BridgeCrossing";
        case Type::BridgeEndFiller:
            return "BridgeEndFiller";
        case Type::RoomCrossing:
            return "RoomCrossing";
        case Type::StairsRoom:
            return "StairsRoom";
        case Type::MonsterThrone:
            return "MonsterThrone";
        case Type::CastleEntrance:
            return "CastleEntrance";
        case Type::CastleStalkRoom:
            return "CastleStalkRoom";
        case Type::CastleSmallCorridor:
            return "CastleSmallCorridorPiece";
        case Type::CastleSmallCorridorCrossing:
            return "CastleSmallCorridorCrossingPiece";
        case Type::CastleSmallCorridorRightTurn:
            return "CastleSmallCorridorRightTurnPiece";
        case Type::CastleSmallCorridorLeftTurn:
            return "CastleSmallCorridorLeftTurnPiece";
        case Type::CastleCorridorStairs:
            return "CastleCorridorStairsPiece";
        case Type::CastleCorridorTBalcony:
            return "CastleCorridorTBalconyPiece";
    }
    return "";
}
