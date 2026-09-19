#pragma once

#include "Core/Math/Vector3f.h"

#include <algorithm>

struct AxisAlignedBB {
    float mMinX = 0.0f;
    float mMinY = 0.0f;
    float mMinZ = 0.0f;
    float mMaxX = 0.0f;
    float mMaxY = 0.0f;
    float mMaxZ = 0.0f;

    AxisAlignedBB() = default;

    AxisAlignedBB(float minX, float minY, float minZ, float maxX, float maxY, float maxZ)
            : mMinX(minX), mMinY(minY), mMinZ(minZ), mMaxX(maxX), mMaxY(maxY), mMaxZ(maxZ) {
    }

    bool isEmpty() const {
        return mMinX >= mMaxX || mMinY >= mMaxY || mMinZ >= mMaxZ;
    }

    AxisAlignedBB offset(float x, float y, float z) const {
        return AxisAlignedBB(mMinX + x, mMinY + y, mMinZ + z, mMaxX + x, mMaxY + y, mMaxZ + z);
    }

    AxisAlignedBB expand(float x, float y, float z) const {
        return AxisAlignedBB(mMinX - x, mMinY - y, mMinZ - z, mMaxX + x, mMaxY + y, mMaxZ + z);
    }

    AxisAlignedBB addCoord(float x, float y, float z) const {
        AxisAlignedBB result = *this;
        if (x < 0.0f)
            result.mMinX += x;
        else
            result.mMaxX += x;

        if (y < 0.0f)
            result.mMinY += y;
        else
            result.mMaxY += y;

        if (z < 0.0f)
            result.mMinZ += z;
        else
            result.mMaxZ += z;

        return result;
    }

    float calculateXOffset(const AxisAlignedBB &other, float x) const {
        if (other.mMaxY <= mMinY || other.mMinY >= mMaxY || other.mMaxZ <= mMinZ || other.mMinZ >= mMaxZ)
            return x;

        if (x > 0.0f && other.mMaxX <= mMinX)
            return std::min(x, mMinX - other.mMaxX);

        if (x < 0.0f && other.mMinX >= mMaxX)
            return std::max(x, mMaxX - other.mMinX);

        return x;
    }

    float calculateYOffset(const AxisAlignedBB &other, float y) const {
        if (other.mMaxX <= mMinX || other.mMinX >= mMaxX || other.mMaxZ <= mMinZ || other.mMinZ >= mMaxZ)
            return y;

        if (y > 0.0f && other.mMaxY <= mMinY)
            return std::min(y, mMinY - other.mMaxY);

        if (y < 0.0f && other.mMinY >= mMaxY)
            return std::max(y, mMaxY - other.mMinY);

        return y;
    }

    float calculateZOffset(const AxisAlignedBB &other, float z) const {
        if (other.mMaxX <= mMinX || other.mMinX >= mMaxX || other.mMaxY <= mMinY || other.mMinY >= mMaxY)
            return z;

        if (z > 0.0f && other.mMaxZ <= mMinZ)
            return std::min(z, mMinZ - other.mMaxZ);

        if (z < 0.0f && other.mMinZ >= mMaxZ)
            return std::max(z, mMaxZ - other.mMinZ);

        return z;
    }

    bool isVectorInside(float x, float y, float z) const {
        return x >= mMinX && x <= mMaxX && y >= mMinY && y <= mMaxY && z >= mMinZ && z <= mMaxZ;
    }

    bool isVectorInside(const Vector3f &position) const {
        return isVectorInside(position.x, position.y, position.z);
    }

    bool intersectsWith(const AxisAlignedBB &other, float epsilon) const {
        if (other.mMaxX - mMinX <= epsilon || mMaxX - other.mMinX <= epsilon)
            return false;

        if (other.mMaxY - mMinY <= epsilon || mMaxY - other.mMinY <= epsilon)
            return false;

        return other.mMaxZ - mMinZ > epsilon && mMaxZ - other.mMinZ > epsilon;
    }

    bool intersectsWith(const AxisAlignedBB &other) const {
        return intersectsWith(other, 1.0E-5f);
    }

    float getAverageEdgeLength() const {
        return ((mMaxX - mMinX) + (mMaxY - mMinY) + (mMaxZ - mMinZ)) / 3.0f;
    }
};
