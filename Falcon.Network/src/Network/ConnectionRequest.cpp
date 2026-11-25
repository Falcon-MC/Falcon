#include "Network/ConnectionRequest.h"

#include <openssl/evp.h>

#include <cstdint>
#include <vector>

namespace {

    const char *EMPTY_GEOMETRY_DATA = "{}";
    const char *GEOMETRY_ENGINE_VERSION = "0.0.0";
    const char *DEFAULT_RESOURCE_PATCH = "{\"geometry\":{\"default\":\"geometry.humanoid.custom\"}}";

    struct PersonaPieceTypeName {
        const char *mSerializeName;
        const char *mTypeName;
    };

    const PersonaPieceTypeName PERSONA_PIECE_TYPES[] = {
            {"unknown", "persona_unknown"},
            {"skeleton", "persona_skeleton"},
            {"body", "persona_body"},
            {"skin", "persona_skin"},
            {"bottom", "persona_bottom"},
            {"feet", "persona_feet"},
            {"dress", "persona_dress"},
            {"top", "persona_top"},
            {"high_pants", "persona_high_pants"},
            {"hands", "persona_hand"},
            {"outerwear", "persona_outerwear"},
            {"facialhair", "persona_facial_hair"},
            {"mouth", "persona_mouth"},
            {"eyes", "persona_eyes"},
            {"hair", "persona_hair"},
            {"hood", "persona_hood"},
            {"back", "persona_back"},
            {"faceaccessory", "persona_face_accessory"},
            {"head", "persona_head"},
            {"legs", "persona_legs"},
            {"leftleg", "persona_left_leg"},
            {"rightleg", "persona_right_leg"},
            {"arms", "persona_arms"},
            {"leftarm", "persona_left_arm"},
            {"rightarm", "persona_right_arm"},
            {"capes", "persona_capes"},
            {"classicskin", "persona_classic_skin"},
            {"emote", "persona_emote"}
    };

    int32_t resolvePersonaPieceType(const std::string &name) {
        const int32_t count = (int32_t) (sizeof(PERSONA_PIECE_TYPES) / sizeof(PERSONA_PIECE_TYPES[0]));
        for (int32_t index = 0; index < count; ++index) {
            if (name == PERSONA_PIECE_TYPES[index].mSerializeName || name == PERSONA_PIECE_TYPES[index].mTypeName)
                return index;
        }

        return 0;
    }

    int decodeBase64Char(char c) {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+' || c == '-') return 62;
        if (c == '/' || c == '_') return 63;
        return -1;
    }

    int hexValue(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    // the client writes colours as unpadded ARGB hex, so "#0" is transparent black and "#b37b62" has no alpha
    int32_t parseArgbColor(const std::string &value) {
        size_t position = !value.empty() && value[0] == '#' ? 1 : 0;
        if (position >= value.size() || value.size() - position > 8)
            return 0;

        uint32_t color = 0;
        for (; position < value.size(); position++) {
            const int digit = hexValue(value[position]);
            if (digit < 0)
                return 0;
            color = (color << 4) | (uint32_t) digit;
        }
        return (int32_t) color;
    }

    std::vector<std::string> extractJsonArrayStrings(const std::string &json, const std::string &key) {
        std::vector<std::string> values;

        const std::string pattern = "\"" + key + "\"";
        size_t position = json.find(pattern);
        if (position == std::string::npos)
            return values;

        position = json.find('[', position + pattern.size());
        if (position == std::string::npos)
            return values;

        bool inString = false;
        std::string value;
        for (size_t i = position + 1; i < json.size(); i++) {
            const char c = json[i];

            if (!inString) {
                if (c == ']')
                    break;
                if (c == '"') {
                    inString = true;
                    value.clear();
                }
                continue;
            }

            if (c == '\\' && i + 1 < json.size()) {
                value.push_back(json[i + 1]);
                i++;
                continue;
            }

            if (c == '"') {
                values.push_back(value);
                inString = false;
                continue;
            }

            value.push_back(c);
        }

        return values;
    }

    std::vector<std::string> extractJsonArrayObjects(const std::string &json, const std::string &key) {
        std::vector<std::string> objects;

        const std::string pattern = "\"" + key + "\"";
        size_t position = json.find(pattern);
        if (position == std::string::npos)
            return objects;

        position = json.find('[', position + pattern.size());
        if (position == std::string::npos)
            return objects;

        int arrayDepth = 0;
        int objectDepth = 0;
        size_t objectStart = std::string::npos;

        for (size_t i = position; i < json.size(); i++) {
            const char c = json[i];

            if (c == '[') {
                arrayDepth++;
                continue;
            }

            if (c == ']') {
                arrayDepth--;
                if (arrayDepth == 0)
                    break;
                continue;
            }

            if (c == '{') {
                if (objectDepth == 0)
                    objectStart = i;
                objectDepth++;
                continue;
            }

            if (c == '}') {
                objectDepth--;
                if (objectDepth == 0 && objectStart != std::string::npos) {
                    objects.push_back(json.substr(objectStart, i - objectStart + 1));
                    objectStart = std::string::npos;
                }
            }
        }

        return objects;
    }

}

std::string ConnectionRequest::decodeBase64Url(const std::string &value) {
    std::string out;
    out.reserve(value.size() * 3 / 4);

    uint32_t buffer = 0;
    int bits = 0;

    for (char c: value) {
        const int digit = decodeBase64Char(c);
        if (digit < 0)
            continue;

        buffer = (buffer << 6) | (uint32_t) digit;
        bits += 6;

        if (bits >= 8) {
            bits -= 8;
            out.push_back((char) ((buffer >> bits) & 0xff));
        }
    }

    return out;
}

std::string ConnectionRequest::readJwtPayload(const std::string &jwt) {
    const size_t first = jwt.find('.');
    if (first == std::string::npos)
        return std::string();

    const size_t second = jwt.find('.', first + 1);
    if (second == std::string::npos)
        return std::string();

    return decodeBase64Url(jwt.substr(first + 1, second - first - 1));
}

int ConnectionRequest::findJsonInt(const std::string &json, const std::string &key, int fallback) {
    const std::string pattern = "\"" + key + "\"";

    size_t position = json.find(pattern);
    if (position == std::string::npos)
        return fallback;

    position = json.find(':', position + pattern.size());
    if (position == std::string::npos)
        return fallback;

    position++;
    while (position < json.size() && (json[position] == ' ' || json[position] == '\t'))
        position++;

    bool negative = false;
    if (position < json.size() && json[position] == '-') {
        negative = true;
        position++;
    }

    bool hasDigit = false;
    int64_t value = 0;
    while (position < json.size() && json[position] >= '0' && json[position] <= '9') {
        value = value * 10 + (json[position] - '0');
        if (value > (int64_t) INT32_MAX)
            return fallback;
        position++;
        hasDigit = true;
    }

    if (!hasDigit)
        return fallback;

    return (int) (negative ? -value : value);
}

bool ConnectionRequest::findJsonBool(const std::string &json, const std::string &key, bool fallback) {
    const std::string pattern = "\"" + key + "\"";

    size_t position = json.find(pattern);
    if (position == std::string::npos)
        return fallback;

    position = json.find(':', position + pattern.size());
    if (position == std::string::npos)
        return fallback;

    position++;
    while (position < json.size() && (json[position] == ' ' || json[position] == '\t'))
        position++;

    if (json.compare(position, 4, "true") == 0)
        return true;

    if (json.compare(position, 5, "false") == 0)
        return false;

    return fallback;
}

std::string ConnectionRequest::findJsonString(const std::string &json, const std::string &key) {
    const std::string pattern = "\"" + key + "\"";

    size_t position = json.find(pattern);
    if (position == std::string::npos)
        return std::string();

    position = json.find(':', position + pattern.size());
    if (position == std::string::npos)
        return std::string();

    position = json.find('"', position);
    if (position == std::string::npos)
        return std::string();

    std::string value;
    for (size_t i = position + 1; i < json.size(); i++) {
        if (json[i] == '\\' && i + 1 < json.size()) {
            value.push_back(json[i + 1]);
            i++;
            continue;
        }

        if (json[i] == '"')
            return value;

        value.push_back(json[i]);
    }

    return std::string();
}

std::string ConnectionRequest::identityFromXuid(const std::string &xuid) {
    const std::string name = "pocket-auth-1-xuid:" + xuid;
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    if (EVP_Digest(name.data(), name.size(), digest, &length, EVP_md5(), nullptr) != 1 || length < 16)
        return std::string();

    digest[6] = (unsigned char) ((digest[6] & 0x0f) | 0x30);
    digest[8] = (unsigned char) ((digest[8] & 0x3f) | 0x80);

    static const char hex[] = "0123456789abcdef";
    std::string result;
    for (int index = 0; index < 16; index++) {
        if (index == 4 || index == 6 || index == 8 || index == 10)
            result.push_back('-');
        result.push_back(hex[digest[index] >> 4]);
        result.push_back(hex[digest[index] & 0x0f]);
    }
    return result;
}

bool ConnectionRequest::parse(const std::string &authJwt, const std::string &clientJwt) {
    // the identity claims live in the last certificate of the chain
    size_t search = 0;
    std::string identityPayload;
    std::string tokenPayload;

    for (;;) {
        const size_t start = authJwt.find("ey", search);
        if (start == std::string::npos)
            break;

        size_t end = start;
        while (end < authJwt.size() && authJwt[end] != '"')
            end++;

        const std::string payload = readJwtPayload(authJwt.substr(start, end - start));
        if (payload.find("extraData") != std::string::npos)
            identityPayload = payload;
        else if (payload.find("\"xid\"") != std::string::npos)
            tokenPayload = payload;

        search = end + 1;
    }

    if (!identityPayload.empty()) {
        mDisplayName = findJsonString(identityPayload, "displayName");
        mIdentity = findJsonString(identityPayload, "identity");
        mXuid = findJsonString(identityPayload, "XUID");
    }

    if (!tokenPayload.empty()) {
        if (mXuid.empty())
            mXuid = findJsonString(tokenPayload, "xid");
        if (mDisplayName.empty())
            mDisplayName = findJsonString(tokenPayload, "xname");
        if (mIdentity.empty()) {
            mIdentity = findJsonString(tokenPayload, "leguuid");
            if (mIdentity.empty() && !mXuid.empty())
                mIdentity = identityFromXuid(mXuid);
        }
    }

    const std::string clientPayload = readJwtPayload(clientJwt);
    mClientData = clientPayload;
    if (!clientPayload.empty()) {
        mDeviceId = findJsonString(clientPayload, "DeviceId");
        mTitleId = findJsonString(clientPayload, "TitleID");
        mBuildPlatform = findJsonInt(clientPayload, "DeviceOS", -1);
        mLanguageCode = findJsonString(clientPayload, "LanguageCode");

        if (mDisplayName.empty())
            mDisplayName = findJsonString(clientPayload, "ThirdPartyName");

        parseSkin(clientPayload);
    } else {
        mSkin = buildDefaultSkin();
    }

    return !mDisplayName.empty();
}

SerializedSkin ConnectionRequest::buildDefaultSkin() const {
    SerializedSkin skin;
    skin.mSkinId = "Falcon.Default.Steve";
    skin.mSkinResourcePatch = "{\"geometry\":{\"default\":\"geometry.humanoid.custom\"}}";
    skin.mSkinData.mWidth = 64;
    skin.mSkinData.mHeight = 64;
    skin.mSkinData.mData.assign(64 * 64 * 4, (char) 0xff);
    skin.mArmsWide = true;
    skin.mFullSkinId = "Falcon.Default.Steve";
    normalizeSkin(skin);
    return skin;
}

void ConnectionRequest::normalizeSkin(SerializedSkin &skin) {
    if (skin.mGeometryData.empty())
        skin.mGeometryData = EMPTY_GEOMETRY_DATA;

    if (skin.mGeometryDataEngineVersion.empty())
        skin.mGeometryDataEngineVersion = GEOMETRY_ENGINE_VERSION;

    if (skin.mSkinResourcePatch.empty())
        skin.mSkinResourcePatch = DEFAULT_RESOURCE_PATCH;

    if (skin.mFullSkinId.empty())
        skin.mFullSkinId = skin.mSkinId;
}

void ConnectionRequest::parseSkin(const std::string &clientPayload) {
    SerializedSkin skin;

    skin.mSkinId = findJsonString(clientPayload, "SkinId");
    mPlayFabId = findJsonString(clientPayload, "PlayFabId");
    skin.mSkinResourcePatch = decodeBase64Url(findJsonString(clientPayload, "SkinResourcePatch"));

    skin.mSkinData.mWidth = findJsonInt(clientPayload, "SkinImageWidth");
    skin.mSkinData.mHeight = findJsonInt(clientPayload, "SkinImageHeight");
    skin.mSkinData.mData = decodeBase64Url(findJsonString(clientPayload, "SkinData"));

    skin.mCapeData.mWidth = findJsonInt(clientPayload, "CapeImageWidth");
    skin.mCapeData.mHeight = findJsonInt(clientPayload, "CapeImageHeight");
    skin.mCapeData.mData = decodeBase64Url(findJsonString(clientPayload, "CapeData"));

    skin.mGeometryData = decodeBase64Url(findJsonString(clientPayload, "SkinGeometryData"));
    skin.mGeometryDataEngineVersion = decodeBase64Url(findJsonString(clientPayload, "SkinGeometryDataEngineVersion"));
    skin.mAnimationData = decodeBase64Url(findJsonString(clientPayload, "SkinAnimationData"));

    skin.mCapeId = findJsonString(clientPayload, "CapeId");
    skin.mFullSkinId = findJsonString(clientPayload, "FullSkinId");
    skin.mArmsWide = findJsonString(clientPayload, "ArmSize") != "slim";
    skin.mColorArgb = parseArgbColor(findJsonString(clientPayload, "SkinColor"));

    skin.mPremium = findJsonBool(clientPayload, "PremiumSkin");
    skin.mPersona = findJsonBool(clientPayload, "PersonaSkin");
    skin.mCapeOnClassic = findJsonBool(clientPayload, "CapeOnClassicSkin");
    skin.mOverridingPlayerAppearance = findJsonBool(clientPayload, "OverrideSkin");
    skin.mTrusted = findJsonBool(clientPayload, "TrustedSkin");
    skin.mProfileHash = findJsonString(clientPayload, "ProfileHash");
    // vanilla sends the server copy of a login skin with the primary user flag cleared
    skin.mPrimaryUser = false;

    for (const std::string &object: extractJsonArrayObjects(clientPayload, "AnimatedImageData")) {
        SkinAnimationData animation;
        animation.mImage.mWidth = findJsonInt(object, "ImageWidth");
        animation.mImage.mHeight = findJsonInt(object, "ImageHeight");
        animation.mImage.mData = decodeBase64Url(findJsonString(object, "Image"));
        animation.mTextureType = findJsonInt(object, "Type");
        animation.mFrames = (float) findJsonInt(object, "Frames");
        animation.mExpressionType = findJsonInt(object, "AnimationExpression");
        skin.mAnimations.push_back(animation);
    }

    for (const std::string &object: extractJsonArrayObjects(clientPayload, "PersonaPieces")) {
        PersonaPieceData piece;
        piece.mId = findJsonString(object, "PieceId");
        piece.mPieceType = resolvePersonaPieceType(findJsonString(object, "PieceType"));
        piece.mPackId = Uuid::fromString(findJsonString(object, "PackId"));
        piece.mProductId = findJsonString(object, "ProductId");
        piece.mIsDefault = findJsonBool(object, "IsDefault");
        skin.mPersonaPieces.push_back(piece);
    }

    for (const std::string &object: extractJsonArrayObjects(clientPayload, "PieceTintColors")) {
        PersonaPieceTintData tint;
        tint.mType = PERSONA_PIECE_TYPES[resolvePersonaPieceType(findJsonString(object, "PieceType"))].mSerializeName;
        tint.mColors.assign(4, 0);
        const std::vector<std::string> colors = extractJsonArrayStrings(object, "Colors");
        for (size_t i = 0; i < colors.size() && i < tint.mColors.size(); i++)
            tint.mColors[i] = parseArgbColor(colors[i]);
        skin.mTintColors.push_back(tint);
    }

    if (skin.mSkinData.mData.empty() || skin.mSkinData.mWidth <= 0 || skin.mSkinData.mHeight <= 0) {
        mSkin = buildDefaultSkin();
        mSkin.mSkinId = skin.mSkinId.empty() ? mSkin.mSkinId : skin.mSkinId;
        return;
    }

    normalizeSkin(skin);
    mSkin = skin;
}
