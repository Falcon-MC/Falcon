#pragma once

class BlockCollisionBoxDescription {
public:
    BlockCollisionBoxDescription() = default;

    BlockCollisionBoxDescription(float minX, float minY, float minZ,
                                 float maxX, float maxY, float maxZ)
            : mMinX(minX), mMinY(minY), mMinZ(minZ),
              mMaxX(maxX), mMaxY(maxY), mMaxZ(maxZ) {}

    bool isUnitCube() const noexcept {
        return mMinX == 0.0f && mMinY == 0.0f && mMinZ == 0.0f &&
               mMaxX == 1.0f && mMaxY == 1.0f && mMaxZ == 1.0f;
    }

    float mMinX = 0.0f;
    float mMinY = 0.0f;
    float mMinZ = 0.0f;
    float mMaxX = 1.0f;
    float mMaxY = 1.0f;
    float mMaxZ = 1.0f;
};
