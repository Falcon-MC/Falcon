#pragma once

#include "Core/Math/Vector3f.h"

#include <cstddef>
#include <vector>

class Path {
public:
    void clear() {
        mWaypoints.clear();
        mIndex = 0;
        mReachesTarget = false;
    }

    void add(const Vector3f &waypoint) {
        mWaypoints.push_back(waypoint);
    }

    bool isDone() const {
        return mIndex >= mWaypoints.size();
    }

    const Vector3f &current() const {
        return mWaypoints[mIndex];
    }

    void advance() {
        mIndex++;
    }

    bool isEmpty() const {
        return mWaypoints.empty();
    }

    bool reachesTarget() const {
        return mReachesTarget;
    }

    void setReachesTarget(bool reachesTarget) {
        mReachesTarget = reachesTarget;
    }

private:
    std::vector<Vector3f> mWaypoints;
    size_t mIndex = 0;
    bool mReachesTarget = false;
};
