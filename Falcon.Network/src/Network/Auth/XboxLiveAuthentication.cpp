#include "Network/Auth/XboxLiveAuthentication.h"

#include "Core/Json/Json.h"
#include "Network/Auth/AuthenticationUtils.h"
#include "Network/Crypto/Base64.h"
#include "Network/Http/HttpClient.h"

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/x509.h>

#include <atomic>

namespace {

    const char *DEVICE_AUTHENTICATE_URL = "https://device.auth.xboxlive.com/device/authenticate";
    const char *DEVICE_AUTHENTICATE_PATH = "/device/authenticate";
    const char *SISU_AUTHORIZE_URL = "https://sisu.xboxlive.com/authorize";
    const char *SISU_AUTHORIZE_PATH = "/authorize";
    const int64_t EXPIRATION_DELTA = 60;
    const int64_t WINDOWS_EPOCH_OFFSET = 11644473600LL;

    std::atomic<int64_t> gServerTimeDelta(0);

    std::string normalizeRelyingParty(const std::string &relyingParty) {
        std::string result = relyingParty;

        while (!result.empty() && result.back() == '/')
            result.pop_back();

        return result;
    }

    std::string encodeRawUrl(const std::string &data) {
        std::string encoded = Base64::encodeUrl(data);

        while (!encoded.empty() && encoded.back() == '=')
            encoded.pop_back();

        return encoded;
    }

    void appendBigEndian64(std::string &out, int64_t value) {
        for (int shift = 56; shift >= 0; shift -= 8)
            out.push_back((char) (unsigned char) (((uint64_t) value >> shift) & 0xff));
    }

    int64_t readTime(const json::Value *value) {
        int64_t result = 0;

        if (value != nullptr)
            AuthenticationUtils::parseIso8601(value->string(), result);

        return result;
    }

}

class XboxLiveAuthentication::ProofKey {
public:
    ProofKey() : mKey(nullptr) {
        EVP_PKEY_CTX *context = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
        if (context == nullptr)
            return;

        if (EVP_PKEY_keygen_init(context) == 1 &&
            EVP_PKEY_CTX_set_ec_paramgen_curve_nid(context, NID_X9_62_prime256v1) == 1)
            EVP_PKEY_keygen(context, &mKey);

        EVP_PKEY_CTX_free(context);

        if (mKey == nullptr)
            return;

        unsigned char *der = nullptr;
        const int length = i2d_PUBKEY(mKey, &der);

        if (length >= 65 && der[length - 65] == 0x04) {
            mX.assign((const char *) der + length - 64, 32);
            mY.assign((const char *) der + length - 32, 32);
        }

        OPENSSL_free(der);
    }

    ~ProofKey() {
        if (mKey != nullptr)
            EVP_PKEY_free(mKey);
    }

    ProofKey(const ProofKey &) = delete;

    ProofKey &operator=(const ProofKey &) = delete;

    bool isValid() const {
        return mKey != nullptr && mX.size() == 32 && mY.size() == 32;
    }

    std::string toJson() const {
        return "{\"alg\":\"ES256\",\"crv\":\"P-256\",\"kty\":\"EC\",\"use\":\"sig\",\"x\":\"" + encodeRawUrl(mX) +
               "\",\"y\":\"" + encodeRawUrl(mY) + "\"}";
    }

    bool sign(const std::string &data, std::string &outSignature) const {
        EVP_MD_CTX *context = EVP_MD_CTX_new();
        if (context == nullptr)
            return false;

        std::string der;
        size_t length = 0;
        bool success = EVP_DigestSignInit(context, nullptr, EVP_sha256(), nullptr, mKey) == 1 &&
                       EVP_DigestSignUpdate(context, data.data(), data.size()) == 1 &&
                       EVP_DigestSignFinal(context, nullptr, &length) == 1;

        if (success) {
            der.resize(length);
            success = EVP_DigestSignFinal(context, (unsigned char *) &der[0], &length) == 1;
            der.resize(length);
        }

        EVP_MD_CTX_free(context);

        if (!success)
            return false;

        const unsigned char *cursor = (const unsigned char *) der.data();
        ECDSA_SIG *signature = d2i_ECDSA_SIG(nullptr, &cursor, (long) der.size());
        if (signature == nullptr)
            return false;

        const BIGNUM *r = nullptr;
        const BIGNUM *s = nullptr;
        ECDSA_SIG_get0(signature, &r, &s);

        outSignature.assign(64, '\0');
        success = BN_bn2binpad(r, (unsigned char *) &outSignature[0], 32) == 32 &&
                  BN_bn2binpad(s, (unsigned char *) &outSignature[32], 32) == 32;

        ECDSA_SIG_free(signature);
        return success;
    }

    bool signRequest(const std::string &path, const std::string &authorization, const std::string &body,
                     std::string &outHeader) const {
        const int64_t now = AuthenticationUtils::currentUnixTime() + gServerTimeDelta.load();
        const int64_t timestamp = (now + WINDOWS_EPOCH_OFFSET) * 10000000LL;

        std::string message;
        message.reserve(body.size() + path.size() + authorization.size() + 32);
        message.append("\x00\x00\x00\x01\x00", 5);
        appendBigEndian64(message, timestamp);
        message.push_back('\0');
        message.append("POST");
        message.push_back('\0');
        message.append(path);
        message.push_back('\0');
        message.append(authorization);
        message.push_back('\0');
        message.append(body);
        message.push_back('\0');

        std::string signature;
        if (!sign(message, signature))
            return false;

        std::string header("\x00\x00\x00\x01", 4);
        appendBigEndian64(header, timestamp);
        header.append(signature);

        outHeader = Base64::encode(header);
        return true;
    }

private:
    EVP_PKEY *mKey;
    std::string mX;
    std::string mY;
};

const char *XboxLiveAuthentication::MULTIPLAYER_RELYING_PARTY = "https://multiplayer.minecraft.net/";
const char *XboxLiveAuthentication::PLAYFAB_RELYING_PARTY = "http://playfab.xboxlive.com/";

bool XboxLiveToken::isValid() const {
    return !mToken.empty() && AuthenticationUtils::currentUnixTime() < mNotAfter - EXPIRATION_DELTA;
}

std::string XboxLiveToken::getAuthorizationHeader() const {
    return "XBL3.0 x=" + mUserHash + ";" + mToken;
}

bool XboxLiveAuthentication::DeviceToken::isValid() const {
    return !mToken.empty() && AuthenticationUtils::currentUnixTime() < mNotAfter - EXPIRATION_DELTA;
}

XboxLiveAuthentication::XboxLiveAuthentication(LiveAuthentication &live) : mLive(live) {
}

XboxLiveAuthentication::~XboxLiveAuthentication() = default;

void XboxLiveAuthentication::updateServerTime(const HttpResponse &response) {
    const std::string date = response.getHeader("date");
    if (date.empty())
        return;

    int64_t serverTime = 0;
    if (!AuthenticationUtils::parseHttpDate(date, serverTime) || serverTime <= 0)
        return;

    gServerTimeDelta.store(serverTime - AuthenticationUtils::currentUnixTime());
}

std::string XboxLiveAuthentication::describeErrorCode(const std::string &code) {
    if (code == "2148916227")
        return "Your account was banned by Xbox for violating one or more Community Standards for Xbox and is unable to be used.";
    if (code == "2148916229")
        return "Your account is currently restricted and your guardian has not given you permission to play online.";
    if (code == "2148916233")
        return "Your account currently does not have an Xbox profile. Please create one at https://signup.live.com/signup";
    if (code == "2148916234")
        return "Your account has not accepted Xbox's Terms of Service. Please login and accept them.";
    if (code == "2148916235")
        return "Your account resides in a region that Xbox has not authorized use from.";
    if (code == "2148916236")
        return "Your account requires proof of age. Please login to https://login.live.com/login.srf and provide proof of age.";
    if (code == "2148916237")
        return "Your account has reached its limit for playtime. Your account has been blocked from logging in.";
    if (code == "2148916238")
        return "The account date of birth is under 18 years and cannot proceed unless the account is added to a family by an adult.";

    return "unknown error code: " + code;
}

bool XboxLiveAuthentication::_requestDeviceToken(std::string &outError) {
    std::unique_ptr<ProofKey> key(new ProofKey());
    if (!key->isValid()) {
        outError = "could not generate the Xbox Live proof key";
        return false;
    }

    const XboxLiveConfig &config = mLive.getConfig();

    const std::string body = "{\"Properties\":{\"AuthMethod\":\"ProofOfPossession\",\"DeviceType\":\"" +
                             json::escape(config.mDeviceType) + "\",\"Id\":\"{" +
                             AuthenticationUtils::generateUuid() + "}\",\"ProofKey\":" + key->toJson() +
                             ",\"Version\":\"" + json::escape(config.mVersion) +
                             "\"},\"RelyingParty\":\"http://auth.xboxlive.com\",\"TokenType\":\"JWT\"}";

    std::string signature;
    if (!key->signRequest(DEVICE_AUTHENTICATE_PATH, std::string(), body, signature)) {
        outError = "could not sign the device token request";
        return false;
    }

    HttpClient::Headers headers;
    headers.emplace_back("x-xbl-contract-version", "1");
    headers.emplace_back("Signature", signature);

    HttpResponse response;
    if (!HttpClient::post(DEVICE_AUTHENTICATE_URL, headers, body, response, outError))
        return false;

    updateServerTime(response);

    if (response.mStatus != 200) {
        outError = "POST " + std::string(DEVICE_AUTHENTICATE_URL) + ": status " + std::to_string(response.mStatus);
        return false;
    }

    std::unique_ptr<json::Value> root = json::parse(response.mBody);
    if (root == nullptr || !root->isObject() || root->get("Token") == nullptr) {
        outError = "device token response is invalid";
        return false;
    }

    mDeviceToken.mToken = root->get("Token")->string();
    mDeviceToken.mNotAfter = readTime(root->get("NotAfter"));
    mProofKey = std::move(key);
    return true;
}

bool XboxLiveAuthentication::_authorize(const LiveToken &liveToken, const std::string &relyingParty,
                                        XboxLiveToken &outToken, std::string &outError) {
    const XboxLiveConfig &config = mLive.getConfig();

    const std::string body = "{\"AccessToken\":\"t=" + json::escape(liveToken.mAccessToken) + "\",\"AppId\":\"" +
                             json::escape(config.mClientId) + "\",\"DeviceToken\":\"" +
                             json::escape(mDeviceToken.mToken) + "\",\"ProofKey\":" + mProofKey->toJson() +
                             ",\"RelyingParty\":\"" + json::escape(relyingParty) +
                             "\",\"Sandbox\":\"RETAIL\",\"SiteName\":\"user.auth.xboxlive.com\","
                             "\"UseModernGamertag\":true}";

    std::string signature;
    if (!mProofKey->signRequest(SISU_AUTHORIZE_PATH, std::string(), body, signature)) {
        outError = "could not sign the Xbox Live authorization request";
        return false;
    }

    HttpClient::Headers headers;
    headers.emplace_back("x-xbl-contract-version", "1");
    headers.emplace_back("Signature", signature);

    HttpResponse response;
    if (!HttpClient::post(SISU_AUTHORIZE_URL, headers, body, response, outError))
        return false;

    updateServerTime(response);

    if (response.mStatus != 200) {
        const std::string errorCode = response.getHeader("x-err");
        outError = "POST " + std::string(SISU_AUTHORIZE_URL) + ": " +
                   (errorCode.empty() ? "status " + std::to_string(response.mStatus) : describeErrorCode(errorCode));
        return false;
    }

    std::unique_ptr<json::Value> root = json::parse(response.mBody);
    const json::Value *authorization = root != nullptr ? root->get("AuthorizationToken") : nullptr;

    if (authorization == nullptr || !authorization->isObject()) {
        outError = "Xbox Live authorization response has no AuthorizationToken";
        return false;
    }

    const json::Value *claims = authorization->get("DisplayClaims");
    const json::Value *userInfo = claims != nullptr ? claims->get("xui") : nullptr;

    if (userInfo == nullptr || !userInfo->isArray() || userInfo->mArray.empty()) {
        outError = "Xbox Live authorization response has no user info in display claims";
        return false;
    }

    const json::Value &user = *userInfo->mArray[0];
    const json::Value *gamerTag = user.get("gtg");
    const json::Value *xuid = user.get("xid");
    const json::Value *userHash = user.get("uhs");
    const json::Value *token = authorization->get("Token");

    outToken.mGamerTag = gamerTag != nullptr ? gamerTag->string() : std::string();
    outToken.mXuid = xuid != nullptr ? xuid->string() : std::string();
    outToken.mUserHash = userHash != nullptr ? userHash->string() : std::string();
    outToken.mToken = token != nullptr ? token->string() : std::string();
    outToken.mIssueInstant = readTime(authorization->get("IssueInstant"));
    outToken.mNotAfter = readTime(authorization->get("NotAfter"));

    if (outToken.mToken.empty() || outToken.mUserHash.empty()) {
        outError = "Xbox Live authorization response is incomplete";
        return false;
    }

    return true;
}

bool XboxLiveAuthentication::requestToken(const std::string &relyingParty, XboxLiveToken &outToken,
                                          std::string &outError) {
    std::lock_guard<std::mutex> guard(mMutex);

    const std::string key = normalizeRelyingParty(relyingParty);
    auto cached = mTokens.find(key);

    if (cached != mTokens.end() && cached->second.isValid()) {
        outToken = cached->second;
        return true;
    }

    LiveToken liveToken;
    if (!mLive.getToken(liveToken, outError)) {
        outError = "request Live Connect token: " + outError;
        return false;
    }

    if ((mProofKey == nullptr || !mDeviceToken.isValid()) && !_requestDeviceToken(outError)) {
        outError = "request device token: " + outError;
        return false;
    }

    if (!_authorize(liveToken, relyingParty, outToken, outError))
        return false;

    mTokens[key] = outToken;
    return true;
}
