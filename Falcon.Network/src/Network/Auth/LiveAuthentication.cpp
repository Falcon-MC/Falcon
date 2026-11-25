#include "Network/Auth/LiveAuthentication.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "Network/Auth/AuthenticationUtils.h"
#include "Network/Auth/XboxLiveAuthentication.h"
#include "Network/Http/HttpClient.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <thread>

namespace {

    const char *LIVE_CONNECT_URL = "https://login.live.com/oauth20_connect.srf";
    const char *LIVE_TOKEN_URL = "https://login.live.com/oauth20_token.srf";
    const char *LIVE_SCOPE = "service::user.auth.xboxlive.com::MBI_SSL";
    const int64_t TOKEN_EXPIRY_DELTA = 60;
    const int POLL_STEP_MS = 100;

    HttpClient::Headers formHeaders() {
        HttpClient::Headers headers;
        headers.emplace_back("Content-Type", "application/x-www-form-urlencoded");
        return headers;
    }

}

bool LiveToken::isValid() const {
    return !mAccessToken.empty() && AuthenticationUtils::currentUnixTime() + TOKEN_EXPIRY_DELTA < mExpiry;
}

LiveAuthentication::LiveAuthentication(const XboxLiveConfig &config, const std::string &cacheFilePath)
        : mConfig(config), mCacheFilePath(cacheFilePath), mCancel(nullptr) {
    loadCache();
}

void LiveAuthentication::setDeviceCodeCallback(const DeviceCodeCallback &callback) {
    std::lock_guard<std::mutex> guard(mMutex);
    mDeviceCodeCallback = callback;
}

void LiveAuthentication::setCancelFlag(const std::atomic<bool> *cancel) {
    std::lock_guard<std::mutex> guard(mMutex);
    mCancel = cancel;
}

bool LiveAuthentication::_isCancelled() const {
    return mCancel != nullptr && mCancel->load();
}

bool LiveAuthentication::_parseTokenResponse(const std::string &body, LiveToken &outToken,
                                             std::string &outErrorCode, std::string &outErrorDescription) {
    std::unique_ptr<json::Value> root = json::parse(body);
    if (root == nullptr || !root->isObject()) {
        outErrorCode = "invalid_response";
        outErrorDescription = "token response is not a JSON object";
        return false;
    }

    const json::Value *error = root->get("error");
    if (error != nullptr && !error->string().empty()) {
        outErrorCode = error->string();

        const json::Value *description = root->get("error_description");
        outErrorDescription = description != nullptr ? description->string() : std::string();
        return false;
    }

    const json::Value *accessToken = root->get("access_token");
    const json::Value *refreshToken = root->get("refresh_token");
    const json::Value *tokenType = root->get("token_type");
    const json::Value *expiresIn = root->get("expires_in");

    outToken.mAccessToken = accessToken != nullptr ? accessToken->string() : std::string();
    outToken.mRefreshToken = refreshToken != nullptr ? refreshToken->string() : std::string();
    outToken.mTokenType = tokenType != nullptr ? tokenType->string() : std::string();
    outToken.mExpiry = AuthenticationUtils::currentUnixTime() +
                       (int64_t) (expiresIn != nullptr ? expiresIn->number() : 0.0);

    if (outToken.mAccessToken.empty()) {
        outErrorCode = "invalid_response";
        outErrorDescription = "token response has no access_token";
        return false;
    }

    return true;
}

bool LiveAuthentication::requestDeviceCodeToken(LiveToken &outToken, std::string &outError) {
    HttpClient::Headers fields;
    fields.emplace_back("client_id", mConfig.mClientId);
    fields.emplace_back("scope", LIVE_SCOPE);
    fields.emplace_back("response_type", "device_code");

    HttpResponse response;
    if (!HttpClient::post(LIVE_CONNECT_URL, formHeaders(), HttpClient::encodeForm(fields), response, outError))
        return false;

    if (response.mStatus != 200) {
        outError = "POST " + std::string(LIVE_CONNECT_URL) + ": status " + std::to_string(response.mStatus);
        return false;
    }

    std::unique_ptr<json::Value> root = json::parse(response.mBody);
    if (root == nullptr || !root->isObject()) {
        outError = "device code response is not a JSON object";
        return false;
    }

    const json::Value *userCode = root->get("user_code");
    const json::Value *deviceCode = root->get("device_code");
    const json::Value *verificationUri = root->get("verification_uri");
    const json::Value *interval = root->get("interval");
    const json::Value *expiresIn = root->get("expires_in");

    if (userCode == nullptr || deviceCode == nullptr || verificationUri == nullptr) {
        outError = "device code response is missing fields";
        return false;
    }

    const int intervalSeconds = interval != nullptr && interval->integer() > 0 ? interval->integer() : 5;
    const int64_t deadline = AuthenticationUtils::currentUnixTime() +
                             (expiresIn != nullptr && expiresIn->integer() > 0 ? expiresIn->integer() : 900);

    if (mDeviceCodeCallback != nullptr) {
        mDeviceCodeCallback(verificationUri->string(), userCode->string());
    } else {
        LOG_INFO(LogAreaID::Network, "Authenticate at %s using the code %s.", verificationUri->string().c_str(),
                 userCode->string().c_str());
    }

    HttpClient::Headers pollFields;
    pollFields.emplace_back("client_id", mConfig.mClientId);
    pollFields.emplace_back("grant_type", "urn:ietf:params:oauth:grant-type:device_code");
    pollFields.emplace_back("device_code", deviceCode->string());
    const std::string pollBody = HttpClient::encodeForm(pollFields);

    for (;;) {
        for (int waited = 0; waited < intervalSeconds * 1000; waited += POLL_STEP_MS) {
            if (_isCancelled()) {
                outError = "device authentication cancelled";
                return false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(POLL_STEP_MS));
        }

        if (AuthenticationUtils::currentUnixTime() > deadline) {
            outError = "device code expired before authentication completed";
            return false;
        }

        HttpResponse poll;
        if (!HttpClient::post(LIVE_TOKEN_URL, formHeaders(), pollBody, poll, outError))
            return false;

        XboxLiveAuthentication::updateServerTime(poll);

        std::string errorCode;
        std::string errorDescription;

        if (_parseTokenResponse(poll.mBody, outToken, errorCode, errorDescription)) {
            LOG_INFO(LogAreaID::Network, "Authentication successful.");
            return true;
        }

        if (errorCode == "authorization_pending")
            continue;

        outError = "error polling for device auth: " + errorCode + ": " + errorDescription;
        return false;
    }
}

bool LiveAuthentication::refreshToken(const LiveToken &token, LiveToken &outToken, std::string &outError) {
    HttpClient::Headers fields;
    fields.emplace_back("client_id", mConfig.mClientId);
    fields.emplace_back("scope", LIVE_SCOPE);
    fields.emplace_back("grant_type", "refresh_token");
    fields.emplace_back("refresh_token", token.mRefreshToken);

    HttpResponse response;
    if (!HttpClient::post(LIVE_TOKEN_URL, formHeaders(), HttpClient::encodeForm(fields), response, outError))
        return false;

    XboxLiveAuthentication::updateServerTime(response);

    std::string errorCode;
    std::string errorDescription;

    if (response.mStatus != 200 || !_parseTokenResponse(response.mBody, outToken, errorCode, errorDescription)) {
        outError = "POST " + std::string(LIVE_TOKEN_URL) + ": refresh error: " + errorCode;
        return false;
    }

    return true;
}

bool LiveAuthentication::getToken(LiveToken &outToken, std::string &outError) {
    std::lock_guard<std::mutex> guard(mMutex);

    if (mToken.isValid()) {
        outToken = mToken;
        return true;
    }

    LiveToken token;
    bool obtained = false;

    if (!mToken.mRefreshToken.empty()) {
        std::string refreshError;
        obtained = refreshToken(mToken, token, refreshError);

        if (!obtained)
            LOG_WARN(LogAreaID::Network, "Could not refresh the cached Live token: %s", refreshError.c_str());
    }

    if (!obtained && !requestDeviceCodeToken(token, outError))
        return false;

    mToken = token;
    saveCache();

    outToken = mToken;
    return true;
}

bool LiveAuthentication::loadCache() {
    if (mCacheFilePath.empty())
        return false;

    std::ifstream file(mCacheFilePath, std::ios::binary);
    if (!file)
        return false;

    std::stringstream content;
    content << file.rdbuf();

    std::unique_ptr<json::Value> root = json::parse(content.str());
    if (root == nullptr || !root->isObject())
        return false;

    const json::Value *accessToken = root->get("access_token");
    const json::Value *refreshToken = root->get("refresh_token");
    const json::Value *tokenType = root->get("token_type");
    const json::Value *expiry = root->get("expiry");

    mToken.mAccessToken = accessToken != nullptr ? accessToken->string() : std::string();
    mToken.mRefreshToken = refreshToken != nullptr ? refreshToken->string() : std::string();
    mToken.mTokenType = tokenType != nullptr ? tokenType->string() : std::string();
    mToken.mExpiry = expiry != nullptr ? (int64_t) expiry->number() : 0;
    return !mToken.mRefreshToken.empty() || !mToken.mAccessToken.empty();
}

bool LiveAuthentication::saveCache() const {
    if (mCacheFilePath.empty())
        return false;

    const std::string temporaryPath = mCacheFilePath + ".tmp";

    {
        std::ofstream file(temporaryPath, std::ios::binary | std::ios::trunc);
        if (!file)
            return false;

        file << "{\"access_token\":\"" << json::escape(mToken.mAccessToken) << "\",\"refresh_token\":\""
             << json::escape(mToken.mRefreshToken) << "\",\"token_type\":\"" << json::escape(mToken.mTokenType)
             << "\",\"expiry\":" << mToken.mExpiry << "}";

        if (!file)
            return false;
    }

    std::remove(mCacheFilePath.c_str());
    return std::rename(temporaryPath.c_str(), mCacheFilePath.c_str()) == 0;
}

void LiveAuthentication::clearCache() {
    std::lock_guard<std::mutex> guard(mMutex);
    mToken = LiveToken();

    if (!mCacheFilePath.empty())
        std::remove(mCacheFilePath.c_str());
}
