#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cmath>

// small helpers that many files used to carry a copy of
namespace VectorMath {
    // the middle of a block
    inline Vector3f centerOf(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }

    // the middle of a block's bottom face
    inline Vector3f bottomCenterOf(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y, (float) position.z + 0.5f);
    }

    inline float distanceSquared(const Vector3f &left, const Vector3f &right) {
        const float dx = left.x - right.x;
        const float dy = left.y - right.y;
        const float dz = left.z - right.z;
        return dx * dx + dy * dy + dz * dz;
    }

    // into [-180, 180)
    inline float wrapDegrees(float degrees) {
        float wrapped = std::fmod(degrees + 180.0f, 360.0f);
        if (wrapped < 0.0f)
            wrapped += 360.0f;
        return wrapped - 180.0f;
    }
}
