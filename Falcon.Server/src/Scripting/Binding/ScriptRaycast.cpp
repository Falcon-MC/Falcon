#include "Scripting/Binding/ScriptRaycast.h"

#include "World/Actor/Movement/MovementConstants.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"

#include "World/Level/Block/BlockShape.h"
#include "World/Level/Block/LiquidBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Level/Block/BlockNames.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace ScriptRaycast {
    bool clampDistance(bool present, double requested, float &distance) {
        if (!present) {
            distance = (float) DEFAULT_DISTANCE;
            return true;
        }
        // the options carry an int: the fraction is dropped
        const int32_t blocks = (int32_t) std::max(-1.0, std::min(requested, (double) MAX_DISTANCE));
        if (blocks <= 0)
            return false;
        distance = (float) blocks;
        return true;
    }

    namespace {
        bool isLiquid(const BlockState &state) {
            return dynamic_cast<const LiquidBlock *>(VanillaBlocks::fromIdentifier(state.mName)) != nullptr;
        }

        const char *faceName(int axis, bool positive) {
            static const char *NAMES[3][2] = {{"West", "East"}, {"Down", "Up"}, {"North", "South"}};
            return NAMES[axis][positive ? 1 : 0];
        }

        // the boxes the ray tests in a cell: collision boxes, else the whole cell for a liquid or passable block
        void boxesOf(Level &level, const BlockState &state, int32_t x, int32_t y, int32_t z,
                     const BlockOptions &options, std::vector<AxisAlignedBB> &boxes) {
            boxes.clear();
            if (state.mName == BlockNames::AIR)
                return;
            if (isLiquid(state)) {
                if (options.mIncludeLiquids)
                    boxes.emplace_back((float) x, (float) y, (float) z, x + 1.0f, y + 1.0f, z + 1.0f);
                return;
            }
            if (BlockShape::hasCollision(state)) {
                BlockShape::appendShapesAt(
                        state, x, y, z, nullptr,
                        [&level](int32_t nx, int32_t ny, int32_t nz) { return level.peekBlockPtr(nx, ny, nz); }, boxes);
                return;
            }
            if (options.mIncludePassable)
                boxes.emplace_back((float) x, (float) y, (float) z, x + 1.0f, y + 1.0f, z + 1.0f);
        }
    }

    bool castBlocks(Level &level, const Vector3f &origin, const Vector3f &direction, const BlockOptions &options,
                    BlockHit &hit) {
        const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y
                                       + direction.z * direction.z);
        if (length < 0.0001f)
            return false;
        const Vector3f unit(direction.x / length, direction.y / length, direction.z / length);
        const float d[3] = {unit.x, unit.y, unit.z};
        const float o[3] = {origin.x, origin.y, origin.z};

        int32_t cell[3], step[3];
        float next[3], delta[3];
        for (int axis = 0; axis < 3; ++axis) {
            cell[axis] = (int32_t) std::floor(o[axis]);
            step[axis] = d[axis] > 0.0f ? 1 : -1;
            delta[axis] = d[axis] == 0.0f ? std::numeric_limits<float>::infinity() : std::fabs(1.0f / d[axis]);
            const float boundary = d[axis] > 0.0f ? (float) cell[axis] + 1.0f : (float) cell[axis];
            next[axis] = d[axis] == 0.0f ? std::numeric_limits<float>::infinity() : (boundary - o[axis]) / d[axis];
        }

        std::vector<AxisAlignedBB> boxes;
        float travelled = 0.0f;
        while (travelled <= options.mMaxDistance) {
            if ((cell[1] < level.getMinY() && step[1] < 0) || (cell[1] > level.getMaxY() && step[1] > 0))
                return false;

            const bool inside = cell[1] >= level.getMinY() && cell[1] <= level.getMaxY();
            const BlockState *state = inside ? level.peekBlockPtr(cell[0], cell[1], cell[2]) : nullptr;
            if (state == nullptr && inside)
                return false;
            if (state != nullptr && (!options.mAccepts || options.mAccepts(*state))) {
                boxesOf(level, *state, cell[0], cell[1], cell[2], options, boxes);
                float best = -1.0f;
                int bestAxis = 1;
                for (const AxisAlignedBB &box: boxes) {
                    int axis = 1;
                    const float at = intersectBox(origin, unit, Vector3f(box.mMinX, box.mMinY, box.mMinZ),
                                                  Vector3f(box.mMaxX, box.mMaxY, box.mMaxZ), &axis);
                    if (at >= 0.0f && at <= options.mMaxDistance && (best < 0.0f || at < best)) {
                        best = at;
                        bestAxis = axis;
                    }
                }
                if (best >= 0.0f) {
                    hit.mBlock = Vector3i(cell[0], cell[1], cell[2]);
                    hit.mFace = faceName(bestAxis, step[bestAxis] < 0);
                    hit.mDistance = best;
                    hit.mFaceLocation = Vector3f(o[0] + d[0] * best - (float) cell[0],
                                                 o[1] + d[1] * best - (float) cell[1],
                                                 o[2] + d[2] * best - (float) cell[2]);
                    return true;
                }
            }

            const int axis = next[0] < next[1] ? (next[0] < next[2] ? 0 : 2) : (next[1] < next[2] ? 1 : 2);
            travelled = next[axis];
            next[axis] += delta[axis];
            cell[axis] += step[axis];
        }
        return false;
    }

    float intersectBox(const Vector3f &origin, const Vector3f &direction, const Vector3f &low, const Vector3f &high,
                       int *enteredAxis) {
        const float o[3] = {origin.x, origin.y, origin.z};
        const float d[3] = {direction.x, direction.y, direction.z};
        const float lo[3] = {low.x, low.y, low.z};
        const float hi[3] = {high.x, high.y, high.z};
        float enter = 0.0f, leave = std::numeric_limits<float>::infinity();
        int entered = 1;
        for (int axis = 0; axis < 3; ++axis) {
            if (std::fabs(d[axis]) < 1e-9f) {
                if (o[axis] < lo[axis] || o[axis] > hi[axis])
                    return -1.0f;
                continue;
            }
            float t1 = (lo[axis] - o[axis]) / d[axis];
            float t2 = (hi[axis] - o[axis]) / d[axis];
            if (t1 > t2)
                std::swap(t1, t2);
            if (t1 > enter) {
                enter = t1;
                entered = axis;
            }
            leave = std::min(leave, t2);
            if (enter > leave)
                return -1.0f;
        }
        if (enteredAxis != nullptr)
            *enteredAxis = entered;
        return enter;
    }
}

ActorSize ScriptRaycast::actorSize(const Actor &actor) {
    if (const ServerPlayer *player = dynamic_cast<const ServerPlayer *>(&actor)) {
        const ActorFlags &flags = player->getFlags();
        float height = MovementPhysics::PLAYER_HEIGHT;
        if (flags.get(ActorFlag::Swimming) || flags.get(ActorFlag::Gliding) || flags.get(ActorFlag::Crawling))
            height = MovementPhysics::PLAYER_CRAWLING_HEIGHT;
        else if (flags.get(ActorFlag::Sneaking))
            height = MovementPhysics::PLAYER_SNEAKING_HEIGHT;
        return ActorSize{MovementPhysics::PLAYER_WIDTH, height};
    }
    if (const ServerActor *entity = dynamic_cast<const ServerActor *>(&actor))
        return entity->getSize();
    // the item entity's setSize(0.25, 0.25)
    return ActorSize{0.25f, 0.25f};
}
