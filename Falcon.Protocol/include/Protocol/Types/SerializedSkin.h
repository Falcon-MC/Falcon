#pragma once

#include "Core/Utility/UUID.h"

#include <cstdint>
#include <string>
#include <vector>

class SkinImageData {
public:
    int32_t mWidth = 0;
    int32_t mHeight = 0;
    std::string mData;
};

class SkinAnimationData {
public:
    SkinImageData mImage;
    int32_t mTextureType = 0;
    float mFrames = 0.0f;
    int32_t mExpressionType = 0;
};

class PersonaPieceData {
public:
    std::string mId;
    int32_t mPieceType = 0;
    Uuid mPackId;
    bool mIsDefault = false;
    std::string mProductId;
};

class PersonaPieceTintData {
public:
    std::string mType;
    std::vector<int32_t> mColors;
};

class SerializedSkin {
public:
    std::string mSkinId;
    std::string mSkinResourcePatch;
    SkinImageData mSkinData;
    std::vector<SkinAnimationData> mAnimations;
    SkinImageData mCapeData;
    std::string mGeometryData;
    std::string mGeometryDataEngineVersion;
    std::string mAnimationData;
    std::string mCapeId;
    std::string mFullSkinId;
    bool mArmsWide = true;
    int32_t mColorArgb = 0;
    std::vector<PersonaPieceData> mPersonaPieces;
    std::vector<PersonaPieceTintData> mTintColors;
    bool mPremium = false;
    bool mPersona = false;
    bool mCapeOnClassic = false;
    bool mPrimaryUser = true;
    bool mOverridingPlayerAppearance = true;
    bool mTrusted = true;
    std::string mProfileHash;
};
