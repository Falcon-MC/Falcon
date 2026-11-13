#include "World/Actor/Movement/BlockCollisionEngine.h"

#include "World/Level/Block/Block.h"
#include "World/Level/Block/BlockShape.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace {
    using Vector = std::array<float, 3>;
    // min x, y, z then max x, y, z
    using Box = std::array<float, 6>;

    constexpr float CONTACT_SNAP = 1.0e-6f;
    constexpr float MAX_MOVE_SQUARED = 256.0f;
    constexpr float MAX_MOVE = 16.0f;
    constexpr float EDGE_INSET = 0.025f;
    constexpr float EDGE_DROP_SCALE = -1.01f;
    constexpr float SEARCH_BELOW = 0.2f;
    constexpr float SEARCH_ABOVE = 0.08f;
    constexpr float ACTOR_SEARCH_MARGIN = 0.25f;
    constexpr float UNLOADED_BARRIER_HEIGHT = 100000.0f;
    constexpr int32_t FLOOR_BARRIER_DEPTH = 40;

    constexpr uint32_t RECOVERY_PENETRATED = 0x2;
    constexpr uint32_t RECOVERY_PENETRATED_AGAIN = 0x4;
    constexpr uint32_t RECOVERY_FORCED = 0x8;
    constexpr uint32_t RECOVERY_PUSHED = 0x10;
    constexpr uint32_t RECOVERY_SUPPRESSING = 0x15;
    constexpr uint32_t RECOVERY_KEPT = 0x19;

    // the game rounds every intermediate to a float: keeps the compiler from fusing a multiply into an add
    float rounded(float value) {
        volatile float stored = value;
        return stored;
    }

    int32_t floorCoordinate(float value) {
        return (int32_t) std::floor(value);
    }

    Box toBox(const AxisAlignedBB &box) {
        return {box.mMinX, box.mMinY, box.mMinZ, box.mMaxX, box.mMaxY, box.mMaxZ};
    }

    AxisAlignedBB toAxisAligned(const Box &box) {
        return AxisAlignedBB(box[0], box[1], box[2], box[3], box[4], box[5]);
    }

    struct Contact {
        float penetration = 0.0f;
        Vector clipped{};
        Vector recovered{};
        int axis = 0;
    };

    // one stationary shape against the moving box: clips the move when they are apart on one axis, pushes the box out
    // along the shallowest axis when they overlap
    Contact collide(const Box &stationary, const Box &moving, const Vector &velocity) {
        Contact result{0.0f, velocity, velocity, 0};
        for (int axis = 0; axis < 3; ++axis) {
            if (stationary[axis + 3] <= stationary[axis])
                return result;
        }

        Vector depths{};
        Vector signedDepths{};
        Vector normals{};
        int separated = 0;
        int separatingAxis = 0;
        for (int axis = 0; axis < 3; ++axis) {
            float lower = moving[axis + 3] - stationary[axis];
            float upper = stationary[axis + 3] - moving[axis];
            if (std::fabs(lower) <= CONTACT_SNAP)
                lower = 0.0f;
            if (std::fabs(upper) <= CONTACT_SNAP)
                upper = 0.0f;
            const float positiveLower = std::max(lower, 0.0f);
            const float positiveUpper = std::max(upper, 0.0f);
            if (positiveLower == 0.0f) {
                normals[axis] = -1.0f;
                signedDepths[axis] = lower;
                ++separated;
                separatingAxis = axis;
            } else if (positiveUpper == 0.0f) {
                normals[axis] = 1.0f;
                signedDepths[axis] = upper;
                ++separated;
                separatingAxis = axis;
            } else if (positiveLower < positiveUpper) {
                normals[axis] = -1.0f;
                signedDepths[axis] = depths[axis] = positiveLower;
            } else {
                normals[axis] = 1.0f;
                signedDepths[axis] = depths[axis] = positiveUpper;
            }
            if (separated > 1)
                return result;
        }

        if (separated == 0) {
            int axis = depths[0] > depths[1] ? 1 : 0;
            if (depths[2] < depths[axis])
                axis = 2;
            result.axis = axis;
            result.penetration = std::min(std::min(depths[0], depths[1]), depths[2]);
            const float displacement = normals[axis] * depths[axis];
            result.recovered[axis] = displacement > 0.0f ? std::max(velocity[axis], displacement)
                                                         : std::min(velocity[axis], displacement);
        } else if (signedDepths[separatingAxis] - normals[separatingAxis] * velocity[separatingAxis] > 0.0f) {
            result.clipped[separatingAxis] = result.recovered[separatingAxis] = normals[separatingAxis]
                                                                                * signedDepths[separatingAxis];
        }
        return result;
    }

    struct Search {
        Vector movement{};
        Box region{};
    };

    Box grown(Box bounds, const Vector &delta) {
        for (int axis = 0; axis < 3; ++axis) {
            if (delta[axis] < 0.0f)
                bounds[axis] += delta[axis];
            else if (delta[axis] > 0.0f)
                bounds[axis + 3] += delta[axis];
        }
        return bounds;
    }

    // the region whose shapes can matter: the swept box, the lifted box of a step-up and the ledge under a sneaker
    Search searchMovement(const Box &box, Vector movement, float stepHeight) {
        const float lengthSquared = rounded(rounded(movement[1] * movement[1]) + rounded(movement[0] * movement[0]))
                                    + rounded(movement[2] * movement[2]);
        if (lengthSquared > MAX_MOVE_SQUARED) {
            const float length = std::sqrt(lengthSquared);
            for (float &value: movement)
                value = rounded(value / length) * MAX_MOVE;
        }

        const Box swept = grown(box, movement);
        const Box raised = grown(box, {0.0f, stepHeight, 0.0f});
        const Box combined = grown(box, {movement[0], stepHeight + movement[1], movement[2]});
        Box inset = box;
        for (int axis: {0, 2}) {
            inset[axis] += EDGE_INSET;
            inset[axis + 3] -= EDGE_INSET;
        }
        for (int axis = 0; axis < 3; ++axis) {
            if (inset[axis] > inset[axis + 3])
                inset[axis] = inset[axis + 3] = (box[axis] + box[axis + 3]) * 0.5f;
        }

        Box edge{};
        for (int axis: {0, 2}) {
            edge[axis] = std::min(std::min(inset[axis] + movement[axis], inset[axis]), swept[axis]);
            edge[axis + 3] = std::max(std::max(inset[axis + 3] + movement[axis], inset[axis + 3]), swept[axis + 3]);
        }
        const float drop = rounded(stepHeight * EDGE_DROP_SCALE);
        edge[1] = std::min(inset[1] + drop, swept[1]);
        edge[4] = std::max(inset[4] + drop, swept[4]);

        Box region{};
        for (int axis = 0; axis < 3; ++axis) {
            region[axis] = std::min(std::min(std::min(raised[axis], swept[axis]), combined[axis]), edge[axis]);
            region[axis + 3] = std::max(std::max(std::max(raised[axis + 3], swept[axis + 3]), combined[axis + 3]),
                                        edge[axis + 3]);
        }
        region[0] = std::min(region[0], box[0]);
        region[2] = std::min(region[2], box[2]);
        region[3] = std::max(region[3], box[3]);
        region[5] = std::max(region[5], box[5]);
        region[1] = std::min(box[1] - rounded(std::fabs(movement[1]) + SEARCH_BELOW), region[1]);
        region[4] = std::max(box[4] + SEARCH_ABOVE, region[4]);
        return {movement, region};
    }

    // how deep a shape may sit in the box and still push it out: nothing once the box already was inside one
    Vector recoveryLimits(const BlockCollisionEngine::RecoveryState &state) {
        const bool suppress = (state.mFlags & RECOVERY_FORCED) == 0 && (state.mFlags & RECOVERY_SUPPRESSING) != 0;
        const float limit = suppress ? 0.0f : 1.0f;
        return {limit, limit, limit};
    }

    void updateRecovery(BlockCollisionEngine::RecoveryState &state, bool penetrated, bool pushedToClosestSpace) {
        state.mFlags = (state.mFlags & ~RECOVERY_PUSHED) | (pushedToClosestSpace ? RECOVERY_PUSHED : 0u);
        if (penetrated)
            state.mFlags |= (state.mFlags & RECOVERY_PENETRATED) != 0 ? RECOVERY_PENETRATED_AGAIN : RECOVERY_PENETRATED;
        else
            state.mFlags &= RECOVERY_KEPT;
    }

    struct Resolved {
        Vector displacement{};
        float penetration = 0.0f;
        Box box{};
    };

    // shapes are visited last to first, as the game walks its list
    template <size_t Count>
    Resolved resolveMoves(const Box &box, const std::array<Vector, Count> &moves, const std::vector<Box> &shapes,
                          const Vector &limits) {
        Resolved result{{}, 0.0f, box};
        for (Vector movement: moves) {
            for (auto shape = shapes.rbegin(); shape != shapes.rend(); ++shape) {
                const Contact contact = collide(*shape, result.box, movement);
                movement = contact.penetration > limits[contact.axis] ? contact.clipped : contact.recovered;
                result.penetration += contact.penetration;
            }
            for (int axis = 0; axis < 3; ++axis) {
                result.box[axis] = movement[axis] + result.box[axis];
                result.box[axis + 3] = movement[axis] + result.box[axis + 3];
                result.displacement[axis] = movement[axis] + result.displacement[axis];
            }
        }
        return result;
    }

    bool shouldStepUp(float height, const Vector &requested, const Vector &resolved, bool onGround, bool canFly) {
        return height > 0.0f && (requested[0] != resolved[0] || requested[2] != resolved[2])
               && (onGround || canFly || (requested[1] < 0.0f && requested[1] != resolved[1]));
    }

    // lift by the step height, move, settle back down; kept only when it lands clear and goes further
    bool tryStepUp(const Box &start, const Vector &requested, float height, const std::vector<Box> &shapes,
                   const Vector &limits, Resolved &normal) {
        std::vector<Box> stepShapes;
        stepShapes.reserve(shapes.size());
        for (const Box &shape: shapes) {
            if (shape[1] < start[4])
                stepShapes.push_back(shape);
        }

        const std::array<Vector, 3> moves{{{0.0f, height, 0.0f}, {requested[0], 0.0f, 0.0f}, {0.0f, 0.0f, requested[2]}}};
        const Resolved up = resolveMoves(start, moves, stepShapes, limits);
        const std::array<Vector, 1> downward{{{0.0f, -up.displacement[1], 0.0f}}};
        const Resolved down = resolveMoves(up.box, downward, stepShapes, limits);
        for (const Box &shape: shapes) {
            if (down.box[0] < shape[3] && down.box[3] > shape[0] && down.box[1] < shape[4] && down.box[4] > shape[1]
                && down.box[2] < shape[5] && down.box[5] > shape[2])
                return false;
        }

        Vector displacement{};
        for (int axis = 0; axis < 3; ++axis)
            displacement[axis] = up.displacement[axis] + down.displacement[axis];
        const float before = rounded(normal.displacement[2] * normal.displacement[2])
                             + rounded(normal.displacement[0] * normal.displacement[0]);
        const float after = rounded(displacement[0] * displacement[0]) + rounded(displacement[2] * displacement[2]);
        if (!(before < after))
            return false;

        normal.box = down.box;
        normal.displacement = displacement;
        return true;
    }

    bool accepted(const Box &box, const Box &region) {
        return box[0] < box[3] && box[1] < box[4] && box[2] < box[5] && box[3] > region[0] && box[0] < region[3]
               && box[4] > region[1] && box[1] < region[4] && box[5] > region[2] && box[2] < region[5];
    }

    // unloaded chunks first, then the terrain one column at a time, then actors that collide like blocks
    bool gatherShapes(Level &level, const Box &region, const AxisAlignedBB &mover, bool descending,
                      const Actor *actor, std::vector<Box> &shapes) {
        bool unloaded = false;
        const int32_t minChunkX = (floorCoordinate(region[0]) - 8) >> 4;
        const int32_t minChunkZ = (floorCoordinate(region[2]) - 8) >> 4;
        const int32_t maxChunkX = (floorCoordinate(region[3]) + 8) >> 4;
        const int32_t maxChunkZ = (floorCoordinate(region[5]) + 8) >> 4;
        for (int32_t z = minChunkZ; z <= maxChunkZ; ++z) {
            for (int32_t x = minChunkX; x <= maxChunkX; ++x) {
                if (level.peekChunkPtr(x, z) != nullptr)
                    continue;
                shapes.push_back({(float) (16 * x), -UNLOADED_BARRIER_HEIGHT, (float) (16 * z), (float) (16 * x + 16),
                                  UNLOADED_BARRIER_HEIGHT, (float) (16 * z + 16)});
                unloaded = true;
            }
        }

        const int32_t lowest = LevelChunk::MIN_Y;
        const int32_t highest = LevelChunk::MAX_Y + 1;
        if ((float) lowest > region[4]) {
            Box floor = region;
            floor[1] = -std::numeric_limits<float>::max();
            floor[4] = (float) (lowest - FLOOR_BARRIER_DEPTH);
            shapes.push_back(floor);
        }

        const int32_t minY = std::max(lowest, floorCoordinate(rounded(region[1] - 1.0f)));
        const int32_t maxY = std::min(highest, floorCoordinate(rounded(region[4] + 1.0f)));
        if (minY < maxY) {
            const BlockShape::Neighbours neighbours = [&level](int32_t x, int32_t y, int32_t z) {
                return level.peekBlockPtr(x, y, z);
            };
            const int32_t minX = floorCoordinate(rounded(region[0] - 1.0f));
            const int32_t maxX = floorCoordinate(rounded(region[3] + 1.0f));
            const int32_t minZ = floorCoordinate(rounded(region[2] - 1.0f));
            const int32_t maxZ = floorCoordinate(rounded(region[5] + 1.0f));
            std::vector<AxisAlignedBB> cell;
            for (int32_t x = minX; x <= maxX; ++x) {
                for (int32_t z = minZ; z <= maxZ; ++z) {
                    if (level.peekChunkPtr(x >> 4, z >> 4) == nullptr)
                        continue;
                    for (int32_t y = minY; y <= maxY; ++y) {
                        const BlockState *state = level.peekBlockPtr(x, y, z);
                        if (state == nullptr)
                            continue;
                        cell.clear();
                        const Block *block = VanillaBlocks::fromIdentifier(state->mName);
                        if (block == nullptr
                            || !block->getMoverCollisionShapes(*state, Vector3i(x, y, z), mover, descending, cell))
                            BlockShape::appendShapesAt(*state, x, y, z, actor, neighbours, cell);
                        for (const AxisAlignedBB &shape: cell) {
                            const Box box = toBox(shape);
                            if (accepted(box, region))
                                shapes.push_back(box);
                        }
                    }
                }
            }
        }

        Box wide{};
        for (int axis = 0; axis < 3; ++axis) {
            const float low = rounded(region[axis] - ACTOR_SEARCH_MARGIN);
            const float high = rounded(region[axis + 3] + ACTOR_SEARCH_MARGIN);
            wide[axis] = std::min(low, high);
            wide[axis + 3] = std::max(low, high);
        }
        std::vector<AxisAlignedBB> actors;
        level.appendCollidableActorBoxes(toAxisAligned(wide), actor, actors);
        for (const AxisAlignedBB &shape: actors) {
            const Box box = toBox(shape);
            if (box[0] < wide[3] && wide[0] < box[3] && box[1] < wide[4] && wide[1] < box[4] && box[2] < wide[5]
                && wide[2] < box[5])
                shapes.push_back(box);
        }
        return unloaded;
    }
}

BlockCollisionEngine::Result BlockCollisionEngine::move(Level &level, const AxisAlignedBB &start, const Vector3f &delta,
                                                        float stepHeight, bool wasOnGround, bool flying,
                                                        bool pushedToClosestSpace, bool descending,
                                                        RecoveryState &recovery, const Actor *actor) {
    const Box box = toBox(start);
    const Search search = searchMovement(box, {delta.x, delta.y, delta.z}, stepHeight);
    std::vector<Box> shapes;
    const bool unloaded = gatherShapes(level, search.region, start, descending, actor, shapes);

    const Vector &requested = search.movement;
    const Vector limits = recoveryLimits(recovery);
    // vertical first, then x, then z: each axis against the box already moved on the previous ones
    const std::array<Vector, 3> moves{{{0.0f, requested[1], 0.0f}, {requested[0], 0.0f, 0.0f}, {0.0f, 0.0f, requested[2]}}};
    Resolved resolved = resolveMoves(box, moves, shapes, limits);

    Result result;
    if (shouldStepUp(stepHeight, requested, resolved.displacement, wasOnGround, flying))
        result.mStepped = tryStepUp(box, requested, stepHeight, shapes, limits, resolved);
    updateRecovery(recovery, resolved.penetration >= CONTACT_SNAP, pushedToClosestSpace);

    result.mRequested = Vector3f(requested[0], requested[1], requested[2]);
    result.mMoved = Vector3f(resolved.displacement[0], resolved.displacement[1], resolved.displacement[2]);
    result.mBox = toAxisAligned(resolved.box);
    result.mPenetration = resolved.penetration;
    result.mKeepsHorizontalVelocity = unloaded;

    // FinalizeMoveSystem: an axis collided when the move fell short by more than one float epsilon
    const float epsilon = std::numeric_limits<float>::epsilon();
    const float shortY = std::fabs(requested[1] - resolved.displacement[1]);
    result.mCollidedX = std::fabs(requested[0] - resolved.displacement[0]) > epsilon;
    result.mCollidedZ = std::fabs(requested[2] - resolved.displacement[2]) > epsilon;
    result.mCollidedY = !(shortY <= epsilon);
    result.mOnGround = result.mCollidedY && requested[1] < 0.0f
                               ? true
                               : wasOnGround && shortY <= epsilon && requested[1] == 0.0f;
    return result;
}

Vector3f BlockCollisionEngine::feetOf(const AxisAlignedBB &box) {
    return Vector3f(rounded(box.mMinX + box.mMaxX) * 0.5f, box.mMinY, rounded(box.mMinZ + box.mMaxZ) * 0.5f);
}
