#include "Block/Blocks/BaseRailBlock.h"

#include "Block/BlockIdentifier.h"
#include "Block/BlockSupport.h"
#include "Block/Systems/RedstoneSystem.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <vector>

namespace {
    enum class RailShape {
        Straight,
        Ascending,
        Curved
    };

    struct RailOrientation {
        int mFirst;
        int mSecond;
        int mAscending;
        RailShape mShape;
    };

    const RailOrientation ORIENTATIONS[10] = {
            {RedstoneFace::NORTH, RedstoneFace::SOUTH, RedstoneFace::NONE,  RailShape::Straight},
            {RedstoneFace::EAST,  RedstoneFace::WEST,  RedstoneFace::NONE,  RailShape::Straight},
            {RedstoneFace::EAST,  RedstoneFace::WEST,  RedstoneFace::EAST,  RailShape::Ascending},
            {RedstoneFace::EAST,  RedstoneFace::WEST,  RedstoneFace::WEST,  RailShape::Ascending},
            {RedstoneFace::NORTH, RedstoneFace::SOUTH, RedstoneFace::NORTH, RailShape::Ascending},
            {RedstoneFace::NORTH, RedstoneFace::SOUTH, RedstoneFace::SOUTH, RailShape::Ascending},
            {RedstoneFace::SOUTH, RedstoneFace::EAST,  RedstoneFace::NONE,  RailShape::Curved},
            {RedstoneFace::SOUTH, RedstoneFace::WEST,  RedstoneFace::NONE,  RailShape::Curved},
            {RedstoneFace::NORTH, RedstoneFace::WEST,  RedstoneFace::NONE,  RailShape::Curved},
            {RedstoneFace::NORTH, RedstoneFace::EAST,  RedstoneFace::NONE,  RailShape::Curved},
    };

    const RailOrientation &orientationOf(int value) {
        if (value < 0 || value > 9)
            return ORIENTATIONS[0];

        return ORIENTATIONS[value];
    }

    bool connectsTo(int orientation, int face) {
        const RailOrientation &info = orientationOf(orientation);
        return info.mFirst == face || info.mSecond == face;
    }

    int straightOrientation(int face) {
        if (face == RedstoneFace::EAST || face == RedstoneFace::WEST)
            return BaseRailBlock::STRAIGHT_EAST_WEST;

        return BaseRailBlock::STRAIGHT_NORTH_SOUTH;
    }

    int ascendingOrientation(int face) {
        switch (face) {
            case RedstoneFace::NORTH:
                return BaseRailBlock::ASCENDING_NORTH;
            case RedstoneFace::SOUTH:
                return BaseRailBlock::ASCENDING_SOUTH;
            case RedstoneFace::WEST:
                return BaseRailBlock::ASCENDING_WEST;
            default:
                return BaseRailBlock::ASCENDING_EAST;
        }
    }

    int curvedOrientation(int first, int second) {
        static const int CANDIDATES[4] = {
                BaseRailBlock::CURVED_SOUTH_EAST,
                BaseRailBlock::CURVED_SOUTH_WEST,
                BaseRailBlock::CURVED_NORTH_WEST,
                BaseRailBlock::CURVED_NORTH_EAST
        };

        for (int candidate: CANDIDATES) {
            if (connectsTo(candidate, first) && connectsTo(candidate, second))
                return candidate;
        }

        return BaseRailBlock::CURVED_SOUTH_EAST;
    }

    int straightOrCurvedOrientation(int first, int second) {
        static const int CANDIDATES[6] = {
                BaseRailBlock::STRAIGHT_NORTH_SOUTH,
                BaseRailBlock::STRAIGHT_EAST_WEST,
                BaseRailBlock::CURVED_SOUTH_EAST,
                BaseRailBlock::CURVED_SOUTH_WEST,
                BaseRailBlock::CURVED_NORTH_WEST,
                BaseRailBlock::CURVED_NORTH_EAST
        };

        for (int candidate: CANDIDATES) {
            if (connectsTo(candidate, first) && connectsTo(candidate, second))
                return candidate;
        }

        return BaseRailBlock::STRAIGHT_NORTH_SOUTH;
    }

    struct RailNeighbour {
        Vector3i mPosition;
        std::string mName;
        int mOrientation;
        int mFace;
    };

    int readOrientation(Level &level, const Vector3i &position) {
        const BlockState state = level.getBlockState(position.x, position.y, position.z);
        return state.mStates.getInt("rail_direction", 0);
    }

    void writeOrientation(ServerNetworkHandler &owner, const Vector3i &position, int orientation) {
        Level &level = owner.getLevel();
        BlockState state = level.getBlockState(position.x, position.y, position.z);
        if (state.mStates.getInt("rail_direction", 0) == orientation)
            return;

        Tag states = state.mStates;
        states.putInt("rail_direction", orientation);

        const BlockState updated(state.mName, states);
        level.setBlockState(position.x, position.y, position.z, updated);
        BlockActionHandler::broadcastBlockUpdate(owner, position, updated);
    }

    std::vector<RailNeighbour> railsAround(Level &level, const Vector3i &position,
                                           const std::vector<int> &faces) {
        std::vector<RailNeighbour> result;

        for (int face: faces) {
            const Vector3i side = RedstoneFace::relative(position, face);
            const int levels[3] = {side.y, side.y + 1, side.y - 1};

            for (int y: levels) {
                const Vector3i candidate(side.x, y, side.z);
                const BlockState state = level.getBlockState(candidate.x, candidate.y, candidate.z);
                if (!BaseRailBlock::matches(state.mName))
                    continue;

                const bool known = std::any_of(result.begin(), result.end(),
                                               [&candidate](const RailNeighbour &entry) {
                                                   return entry.mPosition.x == candidate.x
                                                          && entry.mPosition.y == candidate.y
                                                          && entry.mPosition.z == candidate.z;
                                               });
                if (known)
                    continue;

                RailNeighbour neighbour;
                neighbour.mPosition = candidate;
                neighbour.mName = state.mName;
                neighbour.mOrientation = state.mStates.getInt("rail_direction", 0);
                neighbour.mFace = face;
                result.push_back(neighbour);
            }
        }

        return result;
    }

    const std::vector<int> &horizontalFaces() {
        static const std::vector<int> FACES = {RedstoneFace::SOUTH, RedstoneFace::EAST,
                                               RedstoneFace::WEST, RedstoneFace::NORTH};
        return FACES;
    }

    std::vector<RailNeighbour> railsConnected(Level &level, const Vector3i &position, int orientation) {
        const RailOrientation &info = orientationOf(orientation);
        const std::vector<int> faces = {info.mFirst, info.mSecond};

        std::vector<RailNeighbour> around = railsAround(level, position, faces);
        std::vector<RailNeighbour> connected;
        for (const RailNeighbour &entry: around) {
            if (connectsTo(entry.mOrientation, RedstoneFace::opposite(entry.mFace)))
                connected.push_back(entry);
        }

        return connected;
    }

    int connectSingle(ServerNetworkHandler &owner, const Vector3i &position, const RailNeighbour &other,
                      int face) {
        Level &level = owner.getLevel();
        const int delta = position.y - other.mPosition.y;
        const int opposite = RedstoneFace::opposite(face);
        const std::vector<RailNeighbour> connected = railsConnected(level, other.mPosition,
                                                                    readOrientation(level, other.mPosition));

        if (connected.empty()) {
            writeOrientation(owner, other.mPosition,
                             delta == 1 ? ascendingOrientation(opposite) : straightOrientation(face));
            return delta == -1 ? ascendingOrientation(face) : straightOrientation(face);
        }

        if (connected.size() == 1) {
            const int faceConnected = connected.front().mFace;
            const int otherOrientation = readOrientation(level, other.mPosition);

            if (BaseRailBlock::isAbstract(other.mName) && faceConnected != face) {
                writeOrientation(owner, other.mPosition, curvedOrientation(opposite, faceConnected));
                return delta == -1 ? ascendingOrientation(face) : straightOrientation(face);
            }

            if (faceConnected == face) {
                if (orientationOf(otherOrientation).mShape != RailShape::Ascending) {
                    writeOrientation(owner, other.mPosition,
                                     delta == 1 ? ascendingOrientation(opposite) : straightOrientation(face));
                }
                return delta == -1 ? ascendingOrientation(face) : straightOrientation(face);
            }

            if (connectsTo(otherOrientation, RedstoneFace::NORTH)
                && connectsTo(otherOrientation, RedstoneFace::SOUTH)) {
                writeOrientation(owner, other.mPosition,
                                 delta == 1 ? ascendingOrientation(opposite) : straightOrientation(face));
                return delta == -1 ? ascendingOrientation(face) : straightOrientation(face);
            }
        }

        return BaseRailBlock::STRAIGHT_NORTH_SOUTH;
    }

    int connectPair(ServerNetworkHandler &owner, const Vector3i &position, const RailNeighbour &first,
                    int firstFace, const RailNeighbour &second, int secondFace) {
        connectSingle(owner, position, first, firstFace);
        connectSingle(owner, position, second, secondFace);

        if (RedstoneFace::opposite(firstFace) == secondFace) {
            if (position.y - first.mPosition.y == -1)
                return ascendingOrientation(firstFace);
            if (position.y - second.mPosition.y == -1)
                return ascendingOrientation(secondFace);
        }

        return straightOrCurvedOrientation(firstFace, secondFace);
    }

    const RailNeighbour *findByFace(const std::vector<RailNeighbour> &neighbours, int face) {
        for (const RailNeighbour &entry: neighbours) {
            if (entry.mFace == face)
                return &entry;
        }

        return nullptr;
    }

    bool containsFace(const std::vector<RailNeighbour> &neighbours, int face) {
        return findByFace(neighbours, face) != nullptr;
    }
}

BaseRailBlock::BaseRailBlock(const Block &base) : Block(base) {
}

bool BaseRailBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    (void) blockFace;

    return BlockSupport::isSolidOrCauldron(level.getBlockState(position.x, position.y - 1, position.z));
}

bool BaseRailBlock::matches(const std::string &identifier) {
    return BlockIdentifier::equalsAny(identifier, {"minecraft:rail", "minecraft:golden_rail",
                                                   "minecraft:activator_rail", "minecraft:detector_rail"});
}

bool BaseRailBlock::isAbstract(const std::string &identifier) {
    return identifier == "minecraft:rail";
}

void BaseRailBlock::onPlacing(ServerNetworkHandler &owner, const Vector3i &position, BlockState &state) const {
    if (!state.mStates.contains("rail_direction"))
        return;

    Level &level = owner.getLevel();
    const bool abstractRail = isAbstract(state.mName);

    std::vector<RailNeighbour> around = railsAround(level, position, horizontalFaces());
    std::vector<RailNeighbour> affected;
    for (const RailNeighbour &entry: around) {
        if (railsConnected(level, entry.mPosition, entry.mOrientation).size() != 2)
            affected.push_back(entry);
    }

    if (affected.empty())
        return;

    int distinctFaces = 0;
    for (int face: horizontalFaces()) {
        if (containsFace(affected, face))
            distinctFaces++;
    }

    int orientation = STRAIGHT_NORTH_SOUTH;

    if (affected.size() == 1) {
        orientation = connectSingle(owner, position, affected.front(), affected.front().mFace);
    } else if (distinctFaces == 4) {
        const int firstFace = abstractRail ? RedstoneFace::SOUTH : RedstoneFace::EAST;
        const int secondFace = abstractRail ? RedstoneFace::EAST : RedstoneFace::WEST;
        orientation = connectPair(owner, position, *findByFace(affected, firstFace), firstFace,
                                  *findByFace(affected, secondFace), secondFace);
    } else if (abstractRail) {
        int firstFace = RedstoneFace::NONE;
        int secondFace = RedstoneFace::NONE;

        if (distinctFaces == 2) {
            for (int face: horizontalFaces()) {
                if (!containsFace(affected, face))
                    continue;
                if (firstFace == RedstoneFace::NONE)
                    firstFace = face;
                else if (secondFace == RedstoneFace::NONE)
                    secondFace = face;
            }
        } else {
            static const int CANDIDATES[3] = {CURVED_SOUTH_EAST, CURVED_NORTH_EAST, CURVED_SOUTH_WEST};
            for (int candidate: CANDIDATES) {
                const RailOrientation &info = orientationOf(candidate);
                if (containsFace(affected, info.mFirst) && containsFace(affected, info.mSecond)) {
                    firstFace = info.mFirst;
                    secondFace = info.mSecond;
                    break;
                }
            }
        }

        if (firstFace != RedstoneFace::NONE && secondFace != RedstoneFace::NONE) {
            orientation = connectPair(owner, position, *findByFace(affected, firstFace), firstFace,
                                      *findByFace(affected, secondFace), secondFace);
        } else {
            orientation = connectSingle(owner, position, affected.front(), affected.front().mFace);
        }
    } else {
        int face = affected.front().mFace;
        for (const RailNeighbour &entry: affected) {
            if (entry.mFace > face)
                face = entry.mFace;
        }

        const int opposite = RedstoneFace::opposite(face);
        if (containsFace(affected, opposite)) {
            orientation = connectPair(owner, position, *findByFace(affected, face), face,
                                      *findByFace(affected, opposite), opposite);
        } else {
            orientation = connectSingle(owner, position, *findByFace(affected, face), face);
        }
    }

    if (!abstractRail && orientationOf(orientation).mShape == RailShape::Curved)
        orientation = STRAIGHT_NORTH_SOUTH;

    Tag states = state.mStates;
    states.putInt("rail_direction", orientation);
    state = BlockState(state.mName, states);
}
