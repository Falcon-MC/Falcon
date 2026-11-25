#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "Core/Utility/BinaryStream.h"

#include <string>

enum class StructureRotation : int {
    None = 0,
    Rotate90 = 1,
    Rotate180 = 2,
    Rotate270 = 3
};

enum class StructureMirror : int {
    None = 0,
    X = 1,
    Z = 2,
    XZ = 3
};

enum class StructureAnimationMode : int {
    None = 0,
    Layer = 1,
    Blocks = 2
};

struct StructureSettings {
    std::string mPaletteName;
    bool mIgnoringEntities;
    bool mIgnoringBlocks;
    bool mNonTickingPlayersAndTickingAreasEnabled;
    Vector3i mSize;
    Vector3i mOffset;
    int64_t mLastEditedByActorId;
    StructureRotation mRotation;
    StructureMirror mMirror;
    StructureAnimationMode mAnimationMode;
    float mAnimationSeconds;
    float mIntegrityValue;
    int32_t mIntegritySeed;
    Vector3f mPivot;

    StructureSettings()
            : mIgnoringEntities(false), mIgnoringBlocks(false), mNonTickingPlayersAndTickingAreasEnabled(false),
              mLastEditedByActorId(0), mRotation(StructureRotation::None), mMirror(StructureMirror::None),
              mAnimationMode(StructureAnimationMode::None), mAnimationSeconds(0.0f), mIntegrityValue(0.0f),
              mIntegritySeed(0) {}

    void write(BinaryStream &stream) const;

    void read(ReadOnlyBinaryStream &stream);
};
