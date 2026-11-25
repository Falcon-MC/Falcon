#include "World/Level/Block/BlockClip.h"

#include "World/Level/Block/BlockShape.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace {
    // where the segment enters the box, as a fraction of it; -1 when it misses. axis: the entered face's, -1 when
    // the segment starts inside
    float enterBox(const float from[3], const float delta[3], const AxisAlignedBB &box, int &axis) {
        const float low[3] = {box.mMinX, box.mMinY, box.mMinZ};
        const float high[3] = {box.mMaxX, box.mMaxY, box.mMaxZ};
        float enter = 0.0f;
        float leave = 1.0f;
        axis = -1;
        for (int i = 0; i < 3; ++i) {
            if (delta[i] == 0.0f) {
                if (from[i] < low[i] || from[i] > high[i])
                    return -1.0f;
                continue;
            }
            float slabIn = (low[i] - from[i]) / delta[i];
            float slabOut = (high[i] - from[i]) / delta[i];
            if (slabIn > slabOut)
                std::swap(slabIn, slabOut);
            if (slabIn > enter) {
                enter = slabIn;
                axis = i;
            }
            leave = std::min(leave, slabOut);
            // touching a face while moving away is no hit
            if (enter >= leave)
                return -1.0f;
        }
        return enter;
    }

    // the face a segment moving along delta meets first on that axis
    int32_t faceOf(int axis, const float delta[3]) {
        static const int32_t FACES[3][2] = {{5, 4}, {1, 0}, {3, 2}};
        return FACES[axis][delta[axis] > 0.0f ? 1 : 0];
    }
}

bool BlockClip::clip(Level &level, const Vector3f &from, const Vector3f &to, const Actor *actor, BlockClipHit &hit) {
    const float origin[3] = {from.x, from.y, from.z};
    const float delta[3] = {to.x - from.x, to.y - from.y, to.z - from.z};

    int32_t cell[3], step[3];
    float next[3], span[3];
    for (int axis = 0; axis < 3; ++axis) {
        cell[axis] = (int32_t) std::floor(origin[axis]);
        step[axis] = delta[axis] > 0.0f ? 1 : -1;
        if (delta[axis] == 0.0f) {
            next[axis] = span[axis] = std::numeric_limits<float>::infinity();
            continue;
        }
        span[axis] = std::fabs(1.0f / delta[axis]);
        const float boundary = delta[axis] > 0.0f ? (float) cell[axis] + 1.0f : (float) cell[axis];
        next[axis] = (boundary - origin[axis]) / delta[axis];
    }

    const auto neighbours = [&level](int32_t x, int32_t y, int32_t z) { return level.peekBlockPtr(x, y, z); };
    std::vector<AxisAlignedBB> boxes;
    for (int32_t steps = 0; steps <= MAX_STEPS; ++steps) {
        if (cell[1] >= level.getMinY() && cell[1] <= level.getMaxY()) {
            const BlockState *state = level.peekBlockPtr(cell[0], cell[1], cell[2]);
            if (state == nullptr)
                return false;

            if (BlockShape::hasCollision(*state, actor)) {
                boxes.clear();
                BlockShape::appendShapesAt(*state, cell[0], cell[1], cell[2], actor, neighbours, boxes);
                float best = -1.0f;
                int bestAxis = -1;
                const AxisAlignedBB *bestBox = nullptr;
                for (const AxisAlignedBB &box: boxes) {
                    int axis = -1;
                    const float at = enterBox(origin, delta, box, axis);
                    if (at >= 0.0f && (best < 0.0f || at < best)) {
                        best = at;
                        bestAxis = axis;
                        bestBox = &box;
                    }
                }
                if (bestBox != nullptr) {
                    float point[3] = {origin[0] + delta[0] * best, origin[1] + delta[1] * best,
                                      origin[2] + delta[2] * best};
                    if (bestAxis < 0) {
                        // started inside: the face against its way along the longest axis
                        const float x = std::fabs(delta[0]), y = std::fabs(delta[1]), z = std::fabs(delta[2]);
                        bestAxis = x >= y && x >= z ? 0 : (y >= z ? 1 : 2);
                    } else {
                        // on the face plane exactly: rounding would leave it a hair outside the box
                        const float low[3] = {bestBox->mMinX, bestBox->mMinY, bestBox->mMinZ};
                        const float high[3] = {bestBox->mMaxX, bestBox->mMaxY, bestBox->mMaxZ};
                        point[bestAxis] = delta[bestAxis] > 0.0f ? low[bestAxis] : high[bestAxis];
                    }
                    hit.mBlock = Vector3i(cell[0], cell[1], cell[2]);
                    hit.mPosition = Vector3f(point[0], point[1], point[2]);
                    hit.mFace = faceOf(bestAxis, delta);
                    return true;
                }
            }
        }

        const int axis = next[0] < next[1] ? (next[0] < next[2] ? 0 : 2) : (next[1] < next[2] ? 1 : 2);
        if (next[axis] > 1.0f)
            return false;
        next[axis] += span[axis];
        cell[axis] += step[axis];
    }
    return false;
}
