#pragma once

#include "Protocol/Types/SerializedSkin.h"

#include <string>

// Parses the two JWT blobs carried by LoginPacket. Signatures are not verified, only the client
// claims that the server needs for identity are extracted.
class ConnectionRequest {
public:
    bool parse(const std::string &authJwt, const std::string &clientJwt);

    const std::string &getDisplayName() const { return mDisplayName; }

    const std::string &getIdentity() const { return mIdentity; }

    const std::string &getXuid() const { return mXuid; }

    const std::string &getPlayFabId() const { return mPlayFabId; }

    const std::string &getDeviceId() const { return mDeviceId; }

    const std::string &getTitleId() const { return mTitleId; }

    int getBuildPlatform() const { return mBuildPlatform; }

    const std::string &getLanguageCode() const { return mLanguageCode; }

    const SerializedSkin &getSkin() const { return mSkin; }

    /**
     * The decoded client data payload, as the JSON object the client signed.
     */
    const std::string &getClientData() const { return mClientData; }

    static std::string decodeBase64Url(const std::string &value);

    static std::string readJwtPayload(const std::string &jwt);

    static std::string findJsonString(const std::string &json, const std::string &key);

    /**
     * The UUID the game derives from an XUID when the login only carries a multiplayer token: a version 3 UUID
     * over "pocket-auth-1-xuid:" followed by the XUID.
     */
    static std::string identityFromXuid(const std::string &xuid);

    static int findJsonInt(const std::string &json, const std::string &key, int fallback = 0);

    static bool findJsonBool(const std::string &json, const std::string &key, bool fallback = false);

private:
    void parseSkin(const std::string &clientPayload);

    SerializedSkin buildDefaultSkin() const;

    static void normalizeSkin(SerializedSkin &skin);

    std::string mDisplayName;
    std::string mIdentity;
    std::string mXuid;
    std::string mPlayFabId;
    std::string mDeviceId;
    std::string mTitleId;
    int mBuildPlatform = -1;
    std::string mLanguageCode;
    std::string mClientData;
    SerializedSkin mSkin;
};
