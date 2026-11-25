#pragma once

#include <cstdint>
#include <string>

class KeyPair;

struct ClientIdentityData {
    std::string mDisplayName;
    std::string mIdentity;
    std::string mXuid;
    std::string mTitleId;
    std::string mPlayFabId;
    std::string mPlayFabTitleId;
};

struct ClientData {
    static const int DEVICE_ANDROID = 1;
    static const int INPUT_MODE_TOUCH = 2;

    std::string mAnimatedImageDataJson = "[]";
    std::string mArmSize;
    std::string mCapeData;
    std::string mCapeId;
    int mCapeImageHeight = 0;
    int mCapeImageWidth = 0;
    bool mCapeOnClassicSkin = false;
    int64_t mClientRandomId = 0;
    int mCurrentInputMode = 0;
    int mDefaultInputMode = 0;
    std::string mDeviceModel;
    int mDeviceOS = 0;
    std::string mDeviceId;
    std::string mGameVersion;
    int mGuiScale = 0;
    bool mFilterProfanity = false;
    int mClientEditorConnectionIntent = 0;
    bool mClientIsEditorCapable = false;
    std::string mLanguageCode;
    bool mPersonaSkin = false;
    std::string mPlatformOfflineId;
    std::string mPlatformOnlineId;
    bool mPremiumSkin = false;
    std::string mSelfSignedId;
    std::string mServerAddress;
    std::string mSkinAnimationData;
    std::string mSkinData;
    std::string mSkinGeometry;
    std::string mSkinGeometryVersion;
    std::string mSkinId;
    std::string mPlayFabId;
    int mSkinImageHeight = 0;
    int mSkinImageWidth = 0;
    std::string mSkinResourcePatch;
    std::string mSkinColor;
    std::string mPersonaPiecesJson = "[]";
    std::string mPieceTintColorsJson = "[]";
    std::string mThirdPartyName;
    int mUIProfile = 0;
    bool mTrustedSkin = false;
    bool mOverrideSkin = false;
    bool mCompatibleWithClientSideChunkGen = false;
    int mMaxViewDistance = 0;
    int mMemoryTier = 0;
    int mPlatformType = 0;
    int mGraphicsMode = 0;
    std::string mPartyId;
    std::string mProfileHash;
    bool mIsPartyLeader = false;
    std::string mNonce;
};

class ClientConnectionRequest {
public:
    static void applyIdentityDefaults(ClientIdentityData &identity);

    static void applyClientDefaults(ClientData &data, const std::string &serverAddress, const std::string &displayName,
                                    const std::string &gameVersion);

    static std::string toJson(const ClientData &data);

    static bool createOnline(const std::string &chainJson, const std::string &multiplayerToken, const ClientData &data,
                             const KeyPair &key, bool legacy, std::string &outAuthJson, std::string &outClientJwt,
                             std::string &outError);

    static bool createOffline(const ClientIdentityData &identity, const ClientData &data, const KeyPair &key,
                              bool legacy, std::string &outAuthJson, std::string &outClientJwt,
                              std::string &outError);
};
