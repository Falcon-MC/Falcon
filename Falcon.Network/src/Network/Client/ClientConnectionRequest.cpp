#include "Network/Client/ClientConnectionRequest.h"

#include "Core/Json/Json.h"
#include "Network/Auth/AuthenticationUtils.h"
#include "Network/Crypto/Base64.h"
#include "Network/Crypto/Jwt.h"
#include "Network/Crypto/KeyPair.h"

#include <openssl/rand.h>

#include <memory>
#include <cstdio>
#include <vector>

namespace {

    const int64_t TOKEN_LIFETIME = 6 * 3600;
    const char *MULTIPLAYER_AUDIENCE = "api://auth-minecraft-services/multiplayer";

    const char *DEFAULT_SKIN_RESOURCE_PATCH = "{\n  \"geometry\" : {\n    \"default\" : \"geometry.humanoid.custom\"\n  }\n}";

    const char *DEFAULT_SKIN_GEOMETRY = R"GEOMETRY({"format_version":"1.12.0","minecraft:geometry":[{"bones":[{"name":"body","parent":"waist","pivot":[0.0,24.0,0.0]},{"name":"waist","pivot":[0.0,12.0,0.0]},{"cubes":[{"origin":[-5.0,8.0,3.0],"size":[10,16,1],"uv":[0,0]}],"name":"cape","parent":"body","pivot":[0.0,24.0,3.0],"rotation":[0.0,180.0,0.0]}],"description":{"identifier":"geometry.cape","texture_height":32,"texture_width":64}},{"bones":[{"name":"root","pivot":[0.0,0.0,0.0]},{"cubes":[{"origin":[-4.0,12.0,-2.0],"size":[8,12,4],"uv":[16,16]}],"name":"body","parent":"waist","pivot":[0.0,24.0,0.0]},{"name":"waist","parent":"root","pivot":[0.0,12.0,0.0]},{"cubes":[{"origin":[-4.0,24.0,-4.0],"size":[8,8,8],"uv":[0,0]}],"name":"head","parent":"body","pivot":[0.0,24.0,0.0]},{"name":"cape","parent":"body","pivot":[0.0,24,3.0]},{"cubes":[{"inflate":0.50,"origin":[-4.0,24.0,-4.0],"size":[8,8,8],"uv":[32,0]}],"name":"hat","parent":"head","pivot":[0.0,24.0,0.0]},{"cubes":[{"origin":[4.0,12.0,-2.0],"size":[4,12,4],"uv":[32,48]}],"name":"leftArm","parent":"body","pivot":[5.0,22.0,0.0]},{"cubes":[{"inflate":0.250,"origin":[4.0,12.0,-2.0],"size":[4,12,4],"uv":[48,48]}],"name":"leftSleeve","parent":"leftArm","pivot":[5.0,22.0,0.0]},{"name":"leftItem","parent":"leftArm","pivot":[6.0,15.0,1.0]},{"cubes":[{"origin":[-8.0,12.0,-2.0],"size":[4,12,4],"uv":[40,16]}],"name":"rightArm","parent":"body","pivot":[-5.0,22.0,0.0]},{"cubes":[{"inflate":0.250,"origin":[-8.0,12.0,-2.0],"size":[4,12,4],"uv":[40,32]}],"name":"rightSleeve","parent":"rightArm","pivot":[-5.0,22.0,0.0]},{"locators":{"lead_hold":[-6,15,1]},"name":"rightItem","parent":"rightArm","pivot":[-6,15,1]},{"cubes":[{"origin":[-0.10,0.0,-2.0],"size":[4,12,4],"uv":[16,48]}],"name":"leftLeg","parent":"root","pivot":[1.90,12.0,0.0]},{"cubes":[{"inflate":0.250,"origin":[-0.10,0.0,-2.0],"size":[4,12,4],"uv":[0,48]}],"name":"leftPants","parent":"leftLeg","pivot":[1.90,12.0,0.0]},{"cubes":[{"origin":[-3.90,0.0,-2.0],"size":[4,12,4],"uv":[0,16]}],"name":"rightLeg","parent":"root","pivot":[-1.90,12.0,0.0]},{"cubes":[{"inflate":0.250,"origin":[-3.90,0.0,-2.0],"size":[4,12,4],"uv":[0,32]}],"name":"rightPants","parent":"rightLeg","pivot":[-1.90,12.0,0.0]},{"cubes":[{"inflate":0.250,"origin":[-4.0,12.0,-2.0],"size":[8,12,4],"uv":[16,32]}],"name":"jacket","parent":"body","pivot":[0.0,24.0,0.0]}],"description":{"identifier":"geometry.humanoid.custom","texture_height":64,"texture_width":64,"visible_bounds_height":2,"visible_bounds_offset":[0,1,0],"visible_bounds_width":1}},{"bones":[{"name":"root","pivot":[0.0,0.0,0.0]},{"name":"waist","parent":"root","pivot":[0.0,12.0,0.0]},{"cubes":[{"origin":[-4.0,12.0,-2.0],"size":[8,12,4],"uv":[16,16]}],"name":"body","parent":"waist","pivot":[0.0,24.0,0.0]},{"cubes":[{"origin":[-4.0,24.0,-4.0],"size":[8,8,8],"uv":[0,0]}],"name":"head","parent":"body","pivot":[0.0,24.0,0.0]},{"cubes":[{"inflate":0.50,"origin":[-4.0,24.0,-4.0],"size":[8,8,8],"uv":[32,0]}],"name":"hat","parent":"head","pivot":[0.0,24.0,0.0]},{"cubes":[{"origin":[-3.90,0.0,-2.0],"size":[4,12,4],"uv":[0,16]}],"name":"rightLeg","parent":"root","pivot":[-1.90,12.0,0.0]},{"cubes":[{"inflate":0.250,"origin":[-3.90,0.0,-2.0],"size":[4,12,4],"uv":[0,32]}],"name":"rightPants","parent":"rightLeg","pivot":[-1.90,12.0,0.0]},{"cubes":[{"origin":[-0.10,0.0,-2.0],"size":[4,12,4],"uv":[16,48]}],"mirror":true,"name":"leftLeg","parent":"root","pivot":[1.90,12.0,0.0]},{"cubes":[{"inflate":0.250,"origin":[-0.10,0.0,-2.0],"size":[4,12,4],"uv":[0,48]}],"name":"leftPants","parent":"leftLeg","pivot":[1.90,12.0,0.0]},{"cubes":[{"origin":[4.0,11.50,-2.0],"size":[3,12,4],"uv":[32,48]}],"name":"leftArm","parent":"body","pivot":[5.0,21.50,0.0]},{"cubes":[{"inflate":0.250,"origin":[4.0,11.50,-2.0],"size":[3,12,4],"uv":[48,48]}],"name":"leftSleeve","parent":"leftArm","pivot":[5.0,21.50,0.0]},{"name":"leftItem","parent":"leftArm","pivot":[6,14.50,1]},{"cubes":[{"origin":[-7.0,11.50,-2.0],"size":[3,12,4],"uv":[40,16]}],"name":"rightArm","parent":"body","pivot":[-5.0,21.50,0.0]},{"cubes":[{"inflate":0.250,"origin":[-7.0,11.50,-2.0],"size":[3,12,4],"uv":[40,32]}],"name":"rightSleeve","parent":"rightArm","pivot":[-5.0,21.50,0.0]},{"locators":{"lead_hold":[-6,14.50,1]},"name":"rightItem","parent":"rightArm","pivot":[-6,14.50,1]},{"cubes":[{"inflate":0.250,"origin":[-4.0,12.0,-2.0],"size":[8,12,4],"uv":[16,32]}],"name":"jacket","parent":"body","pivot":[0.0,24.0,0.0]},{"name":"cape","parent":"body","pivot":[0.0,24,-3.0]}],"description":{"identifier":"geometry.humanoid.customSlim","texture_height":64,"texture_width":64,"visible_bounds_height":2,"visible_bounds_offset":[0,1,0],"visible_bounds_width":1}}]})GEOMETRY";

    std::string boolean(bool value) {
        return value ? "true" : "false";
    }

    std::string quoted(const std::string &value) {
        return "\"" + json::escape(value) + "\"";
    }

    std::string lifetimeClaims() {
        const int64_t now = AuthenticationUtils::currentUnixTime();
        return "\"exp\":" + std::to_string(now + TOKEN_LIFETIME) + ",\"nbf\":" + std::to_string(now - TOKEN_LIFETIME);
    }

    int64_t randomPositiveInt64() {
        uint64_t value = 0;

        if (RAND_bytes((unsigned char *) &value, (int) sizeof(value)) != 1)
            value = (uint64_t) AuthenticationUtils::currentUnixTime() * 6364136223846793005ULL + 1442695040888963407ULL;

        return (int64_t) (value & 0x7fffffffffffffffULL);
    }

    std::string encodeRequest(const std::vector<std::string> &chain, const std::string &token,
                              int authenticationType, bool legacy, bool tokenOnly = false) {
        std::string certificate = "{\"chain\":[";

        for (size_t index = 0; index < chain.size(); ++index) {
            if (index > 0)
                certificate.push_back(',');

            certificate += quoted(chain[index]);
        }

        certificate += "]}";

        if (legacy)
            return certificate;

        if (authenticationType == 0 && tokenOnly)
            return "{\"AuthenticationType\":0,\"Token\":" + quoted(token) + "}";

        return "{\"Certificate\":" + quoted(certificate) + ",\"AuthenticationType\":" +
               std::to_string(authenticationType) + ",\"Token\":" + quoted(token) + "}";
    }

}

void ClientConnectionRequest::applyIdentityDefaults(ClientIdentityData &identity) {
    if (identity.mIdentity.empty())
        identity.mIdentity = AuthenticationUtils::generateUuid();

    if (identity.mDisplayName.empty())
        identity.mDisplayName = "Steve";
}

void ClientConnectionRequest::applyClientDefaults(ClientData &data, const std::string &serverAddress,
                                                  const std::string &displayName, const std::string &gameVersion) {
    if (data.mServerAddress.empty()) data.mServerAddress = serverAddress;
    data.mThirdPartyName = displayName;

    if (data.mDeviceOS == 0)
        data.mDeviceOS = ClientData::DEVICE_ANDROID;

    if (data.mDefaultInputMode == 0)
        data.mDefaultInputMode = ClientData::INPUT_MODE_TOUCH;

    if (data.mCurrentInputMode == 0)
        data.mCurrentInputMode = ClientData::INPUT_MODE_TOUCH;

    if (data.mGameVersion.empty())
        data.mGameVersion = gameVersion;

    if (data.mClientRandomId == 0)
        data.mClientRandomId = randomPositiveInt64();

    if (data.mDeviceId.empty()) {
        std::string deviceId = AuthenticationUtils::generateUuid();
        std::string compact;
        compact.reserve(32);

        for (char character: deviceId) {
            if (character != '-')
                compact.push_back(character);
        }

        data.mDeviceId = compact;
    }

    if (data.mLanguageCode.empty())
        data.mLanguageCode = "en_GB";

    if (data.mPlayFabId.empty())
        data.mPlayFabId = AuthenticationUtils::randomHex(8);

    if (data.mSelfSignedId.empty())
        data.mSelfSignedId = AuthenticationUtils::generateUuid();

    if (data.mSkinId.empty())
        data.mSkinId = AuthenticationUtils::generateUuid();

    if (data.mArmSize.empty()) data.mArmSize = "wide";

    if (data.mSkinData.empty()) {
        std::string pixels;
        pixels.reserve(32 * 64 * 4);

        for (int index = 0; index < 32 * 64; ++index)
            pixels.append("\x00\x00\x00\xff", 4);

        data.mSkinData = Base64::encode(pixels);
        data.mSkinImageHeight = 32;
        data.mSkinImageWidth = 64;
    }

    if (data.mSkinResourcePatch.empty())
        data.mSkinResourcePatch = Base64::encode(DEFAULT_SKIN_RESOURCE_PATCH);

    if (data.mSkinGeometry.empty())
        data.mSkinGeometry = Base64::encode(DEFAULT_SKIN_GEOMETRY);

    if (data.mSkinGeometryVersion.empty())
        data.mSkinGeometryVersion = Base64::encode("0.0.0");
}

std::string ClientConnectionRequest::toJson(const ClientData &data) {
    std::string result;
    result.reserve(data.mSkinData.size() + data.mSkinGeometry.size() + data.mCapeData.size() + 2048);

    result += "{\"AnimatedImageData\":" + data.mAnimatedImageDataJson;
    result += ",\"CapeData\":" + quoted(data.mCapeData);
    result += ",\"CapeId\":" + quoted(data.mCapeId);
    result += ",\"CapeImageHeight\":" + std::to_string(data.mCapeImageHeight);
    result += ",\"CapeImageWidth\":" + std::to_string(data.mCapeImageWidth);
    result += ",\"CapeOnClassicSkin\":" + boolean(data.mCapeOnClassicSkin);
    result += ",\"ClientRandomId\":" + std::to_string(data.mClientRandomId);
    result += ",\"CurrentInputMode\":" + std::to_string(data.mCurrentInputMode);
    result += ",\"DefaultInputMode\":" + std::to_string(data.mDefaultInputMode);
    result += ",\"DeviceModel\":" + quoted(data.mDeviceModel);
    result += ",\"DeviceOS\":" + std::to_string(data.mDeviceOS);
    result += ",\"DeviceId\":" + quoted(data.mDeviceId);
    result += ",\"GameVersion\":" + quoted(data.mGameVersion);
    result += ",\"GuiScale\":" + std::to_string(data.mGuiScale);
    result += ",\"FilterProfanity\":" + boolean(data.mFilterProfanity);
    result += ",\"ClientEditorConnectionIntent\":" + std::to_string(data.mClientEditorConnectionIntent);
    result += ",\"ClientIsEditorCapable\":" + boolean(data.mClientIsEditorCapable);
    result += ",\"LanguageCode\":" + quoted(data.mLanguageCode);
    result += ",\"PersonaSkin\":" + boolean(data.mPersonaSkin);
    result += ",\"PlatformOfflineId\":" + quoted(data.mPlatformOfflineId);
    result += ",\"PlatformOnlineId\":" + quoted(data.mPlatformOnlineId);
    result += ",\"PremiumSkin\":" + boolean(data.mPremiumSkin);
    result += ",\"SelfSignedId\":" + quoted(data.mSelfSignedId);
    result += ",\"ServerAddress\":" + quoted(data.mServerAddress);
    result += ",\"SkinAnimationData\":" + quoted(data.mSkinAnimationData);
    result += ",\"SkinData\":" + quoted(data.mSkinData);
    result += ",\"SkinGeometryData\":" + quoted(data.mSkinGeometry);
    result += ",\"SkinGeometryDataEngineVersion\":" + quoted(data.mSkinGeometryVersion);
    result += ",\"SkinId\":" + quoted(data.mSkinId);
    result += ",\"PlayFabId\":" + quoted(data.mPlayFabId);
    result += ",\"SkinImageHeight\":" + std::to_string(data.mSkinImageHeight);
    result += ",\"SkinImageWidth\":" + std::to_string(data.mSkinImageWidth);
    result += ",\"SkinResourcePatch\":" + quoted(data.mSkinResourcePatch);
    result += ",\"SkinColor\":" + quoted(data.mSkinColor);
    result += ",\"ArmSize\":" + quoted(data.mArmSize);
    result += ",\"PersonaPieces\":" + data.mPersonaPiecesJson;
    result += ",\"PieceTintColors\":" + data.mPieceTintColorsJson;
    result += ",\"ThirdPartyName\":" + quoted(data.mThirdPartyName);
    result += ",\"UIProfile\":" + std::to_string(data.mUIProfile);
    result += ",\"TrustedSkin\":" + boolean(data.mTrustedSkin);
    result += ",\"OverrideSkin\":" + boolean(data.mOverrideSkin);
    result += ",\"CompatibleWithClientSideChunkGen\":" + boolean(data.mCompatibleWithClientSideChunkGen);
    result += ",\"MaxViewDistance\":" + std::to_string(data.mMaxViewDistance);
    result += ",\"MemoryTier\":" + std::to_string(data.mMemoryTier);
    result += ",\"PlatformType\":" + std::to_string(data.mPlatformType);
    result += ",\"GraphicsMode\":" + std::to_string(data.mGraphicsMode);
    result += ",\"PartyId\":" + quoted(data.mPartyId);
    result += ",\"ProfileHash\":" + quoted(data.mProfileHash);
    result += ",\"IsPartyLeader\":" + boolean(data.mIsPartyLeader);

    if (!data.mNonce.empty())
        result += ",\"Nonce\":" + quoted(data.mNonce);

    result += "}";
    return result;
}

bool ClientConnectionRequest::createOnline(const std::string &chainJson, const std::string &multiplayerToken,
                                           const ClientData &data, const KeyPair &key, bool legacy,
                                           std::string &outAuthJson, std::string &outClientJwt,
                                           std::string &outError) {
    std::unique_ptr<json::Value> root = json::parse(chainJson);
    const json::Value *chain = root != nullptr ? root->get("chain") : nullptr;

    if (chain == nullptr || !chain->isArray() || chain->mArray.empty()) {
        outError = "the login chain is empty";
        return false;
    }

    Jwt::Token first;
    if (!Jwt::parse(chain->mArray[0]->string(), first)) {
        outError = "the first token of the login chain could not be parsed";
        return false;
    }

    const std::string x5u = Jwt::readX5u(first);
    if (x5u.empty()) {
        outError = "the first token of the login chain has no x5u header";
        return false;
    }

    const std::string rootClaims = "{" + lifetimeClaims() + ",\"identityPublicKey\":" + quoted(x5u) +
                                   ",\"certificateAuthority\":true}";
    const std::string rootToken = Jwt::sign(rootClaims, key);

    if (rootToken.empty()) {
        outError = "could not sign the login chain root token";
        return false;
    }

    std::vector<std::string> tokens;
    tokens.reserve(chain->mArray.size() + 1);
    tokens.push_back(rootToken);

    for (const std::unique_ptr<json::Value> &entry: chain->mArray)
        tokens.push_back(entry->string());

    int major = 0, minor = 0, patch = 0;
    const bool tokenOnly = std::sscanf(data.mGameVersion.c_str(), "%d.%d.%d", &major, &minor, &patch) == 3
        && (major > 1 || (major == 1 && (minor > 26 || (minor == 26 && patch >= 30))));
    outAuthJson = encodeRequest(tokens, multiplayerToken, 0, legacy, tokenOnly);
    outClientJwt = Jwt::sign(toJson(data), key);

    if (outClientJwt.empty()) {
        outError = "could not sign the client data token";
        return false;
    }

    return true;
}

bool ClientConnectionRequest::createOffline(const ClientIdentityData &identity, const ClientData &data,
                                            const KeyPair &key, bool legacy, std::string &outAuthJson,
                                            std::string &outClientJwt, std::string &outError) {
    const std::string &publicKey = key.getPublicKeyBase64();

    std::vector<std::string> tokens;
    std::string token;

    if (legacy) {
        std::string extraData = "{\"XUID\":" + quoted(identity.mXuid) + ",\"identity\":" + quoted(identity.mIdentity) +
                                ",\"displayName\":" + quoted(identity.mDisplayName);

        if (!identity.mTitleId.empty())
            extraData += ",\"titleId\":" + quoted(identity.mTitleId);

        extraData += "}";

        const std::string claims = "{" + lifetimeClaims() + ",\"extraData\":" + extraData +
                                   ",\"identityPublicKey\":" + quoted(publicKey) + "}";
        const std::string chainToken = Jwt::sign(claims, key);

        if (chainToken.empty()) {
            outError = "could not sign the offline identity token";
            return false;
        }

        tokens.push_back(chainToken);
    } else {
        std::string claims = "{\"aud\":" + quoted(MULTIPLAYER_AUDIENCE) + "," + lifetimeClaims() +
                             ",\"ipt\":\"\",\"mid\":" + quoted(identity.mPlayFabId) + ",\"tid\":" +
                             quoted(identity.mPlayFabTitleId) + ",\"cpk\":" + quoted(publicKey) + ",\"xid\":" +
                             quoted(identity.mXuid) + ",\"xname\":" + quoted(identity.mDisplayName);

        if (!identity.mIdentity.empty())
            claims += ",\"leguuid\":" + quoted(identity.mIdentity);

        claims += "}";

        token = Jwt::sign(claims, key);
        if (token.empty()) {
            outError = "could not sign the offline multiplayer token";
            return false;
        }

    }

    outAuthJson = encodeRequest(tokens, token, 2, legacy);
    outClientJwt = Jwt::sign(toJson(data), key);

    if (outClientJwt.empty()) {
        outError = "could not sign the client data token";
        return false;
    }

    return true;
}
