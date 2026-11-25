#include "Network/Auth/MinecraftAuthentication.h"

#include "Core/Json/Json.h"
#include "Network/Auth/AuthenticationUtils.h"
#include "Network/Crypto/Jwt.h"
#include "Network/Crypto/KeyPair.h"
#include "Network/Http/HttpClient.h"

#include <cctype>

namespace {

    const char *DISCOVERY_URL = "https://client.discovery.minecraft-services.net/api/v1.0/discovery/MinecraftPE/builds/";
    const char *MINECRAFT_AUTH_URL = "https://multiplayer.minecraft.net/authentication";
    const char *SERVICE_USER_AGENT = "libhttpclient/1.0.0.0";
    const int64_t EXPIRATION_DELTA = 60;

    std::string toLower(const std::string &value) {
        std::string result;
        result.reserve(value.size());

        for (char character: value)
            result.push_back((char) tolower((unsigned char) character));

        return result;
    }

    std::string describeServiceError(const std::string &request, const HttpResponse &response) {
        std::unique_ptr<json::Value> root = json::parse(response.mBody);

        if (root != nullptr && root->isObject()) {
            const json::Value *code = root->get("code");
            const json::Value *message = root->get("message");

            if (code != nullptr || message != nullptr) {
                return request + ": " + (code != nullptr ? code->string() : std::string()) + ": " +
                       (message != nullptr ? message->string() : std::string());
            }
        }

        return request + ": status " + std::to_string(response.mStatus) + ": " + response.mBody.substr(0, 512);
    }

    HttpClient::Headers serviceHeaders() {
        HttpClient::Headers headers;
        headers.emplace_back("Content-Type", "application/json");
        headers.emplace_back("Accept", "application/json");
        headers.emplace_back("User-Agent", SERVICE_USER_AGENT);
        return headers;
    }

}

MinecraftAuthentication::MinecraftAuthentication(const XboxLiveConfig &config, const std::string &cacheFilePath,
                                                 const std::string &gameVersion)
        : mLive(config, cacheFilePath), mXbox(mLive), mGameVersion(gameVersion), mServiceValidUntil(0) {
}

bool MinecraftAuthentication::_discover(std::string &outError) {
    if (mEnvironment.mLoaded)
        return true;

    HttpClient::Headers headers;
    headers.emplace_back("Content-Type", "application/json");
    headers.emplace_back("User-Agent", SERVICE_USER_AGENT);

    const std::string url = std::string(DISCOVERY_URL) + mGameVersion;

    HttpResponse response;
    if (!HttpClient::get(url, headers, response, outError))
        return false;

    if (response.mStatus != 200) {
        outError = describeServiceError("GET " + url, response);
        return false;
    }

    std::unique_ptr<json::Value> root = json::parse(response.mBody);
    const json::Value *result = root != nullptr ? root->get("result") : nullptr;
    const json::Value *environments = result != nullptr ? result->get("serviceEnvironments") : nullptr;
    const json::Value *auth = environments != nullptr ? environments->get("auth") : nullptr;
    const json::Value *production = auth != nullptr ? auth->get("prod") : nullptr;

    if (production == nullptr || !production->isObject()) {
        outError = "\"auth\" is not present on \"prod\" in the discovery service environments";
        return false;
    }

    const json::Value *serviceUri = production->get("serviceUri");
    const json::Value *issuer = production->get("issuer");
    const json::Value *playFabTitleId = production->get("playfabTitleId");

    mEnvironment.mServiceUri = serviceUri != nullptr ? serviceUri->string() : std::string();
    mEnvironment.mIssuer = issuer != nullptr ? issuer->string() : std::string();
    mEnvironment.mPlayFabTitleId = playFabTitleId != nullptr ? playFabTitleId->string() : std::string();

    if (mEnvironment.mServiceUri.empty() || mEnvironment.mPlayFabTitleId.empty()) {
        outError = "the authorization environment is incomplete";
        return false;
    }

    mEnvironment.mDiscoveryBody = response.mBody;
    mEnvironment.mLoaded = true;
    return true;
}

bool MinecraftAuthentication::_loginPlayFab(std::string &outSessionTicket, std::string &outError) {
    XboxLiveToken token;
    if (!mXbox.requestToken(XboxLiveAuthentication::PLAYFAB_RELYING_PARTY, token, outError)) {
        outError = "request xsts token for \"" + std::string(XboxLiveAuthentication::PLAYFAB_RELYING_PARTY) +
                   "\": " + outError;
        return false;
    }

    const std::string url = "https://" + toLower(mEnvironment.mPlayFabTitleId) + ".playfabapi.com/Client/LoginWithXbox";
    const std::string body = "{\"TitleId\":\"" + json::escape(mEnvironment.mPlayFabTitleId) +
                             "\",\"CreateAccount\":true,\"XboxToken\":\"" +
                             json::escape(token.getAuthorizationHeader()) + "\"}";

    HttpClient::Headers headers;
    headers.emplace_back("Content-Type", "application/json");

    HttpResponse response;
    if (!HttpClient::post(url, headers, body, response, outError))
        return false;

    std::unique_ptr<json::Value> root = json::parse(response.mBody);

    if (response.mStatus != 200 && response.mStatus != 201) {
        const json::Value *message = root != nullptr ? root->get("errorMessage") : nullptr;
        outError = "login playfab: status " + std::to_string(response.mStatus) +
                   (message != nullptr ? ": " + message->string() : std::string());
        return false;
    }

    const json::Value *data = root != nullptr ? root->get("data") : nullptr;
    const json::Value *ticket = data != nullptr ? data->get("SessionTicket") : nullptr;

    if (ticket == nullptr || ticket->string().empty()) {
        outError = "login playfab: invalid login result";
        return false;
    }

    outSessionTicket = ticket->string();
    return true;
}

std::string MinecraftAuthentication::_userConfigJson(const std::string &sessionTicket) const {
    return "{\"language\":\"en\",\"languageCode\":\"en-US\",\"regionCode\":\"US\",\"token\":\"" +
           json::escape(sessionTicket) + "\",\"tokenType\":\"PlayFab\"}";
}

bool MinecraftAuthentication::_readServiceToken(const std::string &body, std::string &outError) {
    std::unique_ptr<json::Value> root = json::parse(body);
    const json::Value *result = root != nullptr ? root->get("result") : nullptr;
    const json::Value *authorization = result != nullptr ? result->get("authorizationHeader") : nullptr;
    const json::Value *validUntil = result != nullptr ? result->get("validUntil") : nullptr;

    int64_t expiry = 0;
    if (authorization == nullptr || authorization->string().empty() || validUntil == nullptr ||
        !AuthenticationUtils::parseIso8601(validUntil->string(), expiry) ||
        AuthenticationUtils::currentUnixTime() >= expiry - EXPIRATION_DELTA) {
        outError = "invalid token result from the authorization service";
        return false;
    }

    mServiceAuthorization = authorization->string();
    mServiceValidUntil = expiry;
    return true;
}

bool MinecraftAuthentication::_serviceToken(std::string &outAuthorization, std::string &outError) {
    if (!mServiceAuthorization.empty() &&
        AuthenticationUtils::currentUnixTime() < mServiceValidUntil - EXPIRATION_DELTA) {
        outAuthorization = mServiceAuthorization;
        return true;
    }

    if (!_discover(outError)) {
        outError = "obtain environment for auth: " + outError;
        return false;
    }

    std::string sessionTicket;
    if (!_loginPlayFab(sessionTicket, outError))
        return false;

    HttpClient::Headers headers = serviceHeaders();
    HttpResponse response;

    if (mServiceAuthorization.empty()) {
        const std::string url = AuthenticationUtils::joinUrl(mEnvironment.mServiceUri, "/api/v1.0/session/start");
        const std::string body = "{\"device\":{\"applicationType\":\"MinecraftPE\",\"capabilities\":[],"
                                 "\"gameVersion\":\"" + json::escape(mGameVersion) + "\",\"id\":\"" +
                                 AuthenticationUtils::generateUuid() + "\",\"memory\":\"17179869184\","
                                 "\"platform\":\"Windows10\",\"playFabTitleId\":\"" +
                                 json::escape(mEnvironment.mPlayFabTitleId) + "\",\"storePlatform\":\"uwp.store\","
                                 "\"type\":\"Windows10\"},\"user\":" + _userConfigJson(sessionTicket) + "}";

        if (!HttpClient::post(url, headers, body, response, outError))
            return false;

        if (response.mStatus != 200) {
            outError = describeServiceError("POST " + url, response);
            return false;
        }
    } else {
        const std::string url = AuthenticationUtils::joinUrl(mEnvironment.mServiceUri, "/api/v1.0/session/renew");
        headers.emplace_back("Authorization", mServiceAuthorization);

        if (!HttpClient::post(url, headers, _userConfigJson(sessionTicket), response, outError))
            return false;

        if (response.mStatus != 200) {
            outError = "renew: " + describeServiceError("POST " + url, response);
            return false;
        }
    }

    if (!_readServiceToken(response.mBody, outError))
        return false;

    outAuthorization = mServiceAuthorization;
    return true;
}

bool MinecraftAuthentication::requestMultiplayerToken(const KeyPair &key, std::string &outToken,
                                                      std::string &outError) {
    std::lock_guard<std::mutex> guard(mMutex);

    std::string authorization;
    if (!_serviceToken(authorization, outError)) {
        outError = "request service token: " + outError;
        return false;
    }

    const std::string url = AuthenticationUtils::joinUrl(mEnvironment.mServiceUri,
                                                         "/api/v1.0/multiplayer/session/start");
    const std::string body = "{\"publicKey\":\"" + json::escape(key.getPublicKeyBase64()) + "\"}";

    HttpClient::Headers headers;
    headers.emplace_back("Content-Type", "application/json");
    headers.emplace_back("Accept", "application/json");
    headers.emplace_back("Authorization", authorization);

    HttpResponse response;
    if (!HttpClient::post(url, headers, body, response, outError))
        return false;

    if (response.mStatus != 200) {
        outError = describeServiceError("POST " + url, response);
        return false;
    }

    std::unique_ptr<json::Value> root = json::parse(response.mBody);
    const json::Value *result = root != nullptr ? root->get("result") : nullptr;
    const json::Value *signedToken = result != nullptr ? result->get("signedToken") : nullptr;
    const json::Value *validUntil = result != nullptr ? result->get("validUntil") : nullptr;

    int64_t expiry = 0;
    if (signedToken == nullptr || signedToken->string().empty() || validUntil == nullptr ||
        !AuthenticationUtils::parseIso8601(validUntil->string(), expiry) ||
        AuthenticationUtils::currentUnixTime() >= expiry) {
        outError = "invalid multiplayer token result";
        return false;
    }

    outToken = signedToken->string();
    return true;
}

bool MinecraftAuthentication::requestServiceToken(std::string &outAuthorization, std::string &outError) {
    std::lock_guard<std::mutex> guard(mMutex);

    if (!_serviceToken(outAuthorization, outError)) {
        outError = "request service token: " + outError;
        return false;
    }

    return true;
}

bool MinecraftAuthentication::requestServiceUri(const std::string &serviceName, std::string &outServiceUri,
                                                std::string &outError) {
    std::lock_guard<std::mutex> guard(mMutex);

    if (!_discover(outError)) {
        outError = "obtain environment for " + serviceName + ": " + outError;
        return false;
    }

    std::unique_ptr<json::Value> root = json::parse(mEnvironment.mDiscoveryBody);
    const json::Value *result = root != nullptr ? root->get("result") : nullptr;
    const json::Value *environments = result != nullptr ? result->get("serviceEnvironments") : nullptr;
    const json::Value *service = environments != nullptr ? environments->get(serviceName) : nullptr;
    const json::Value *production = service != nullptr ? service->get("prod") : nullptr;
    const json::Value *serviceUri = production != nullptr ? production->get("serviceUri") : nullptr;

    if (serviceUri == nullptr || serviceUri->string().empty()) {
        outError = "\"" + serviceName + "\" has no \"serviceUri\" on \"prod\" in the discovery service environments";
        return false;
    }

    outServiceUri = serviceUri->string();
    return true;
}

bool MinecraftAuthentication::requestChain(const KeyPair &key, std::string &outChainJson, std::string &outError) {
    std::lock_guard<std::mutex> guard(mMutex);

    XboxLiveToken token;
    if (!mXbox.requestToken(XboxLiveAuthentication::MULTIPLAYER_RELYING_PARTY, token, outError)) {
        outError = "request XBOX Live token: " + outError;
        return false;
    }

    const std::string body = "{\"identityPublicKey\":\"" + key.getPublicKeyBase64() + "\"}";

    HttpClient::Headers headers;
    headers.emplace_back("Authorization", token.getAuthorizationHeader());
    headers.emplace_back("User-Agent", "MCPE/Android");
    headers.emplace_back("Client-Version", mGameVersion);
    headers.emplace_back("Content-Type", "application/json");

    HttpResponse response;
    if (!HttpClient::post(MINECRAFT_AUTH_URL, headers, body, response, outError))
        return false;

    if (response.mStatus != 200) {
        outError = "POST " + std::string(MINECRAFT_AUTH_URL) + ": status " + std::to_string(response.mStatus);
        return false;
    }

    outChainJson = response.mBody;
    return true;
}

bool MinecraftAuthentication::readChainIdentity(const std::string &chainJson, MinecraftAuthenticationResult &outResult,
                                                std::string &outError) {
    std::unique_ptr<json::Value> root = json::parse(chainJson);
    const json::Value *chain = root != nullptr ? root->get("chain") : nullptr;

    if (chain == nullptr || !chain->isArray() || chain->mArray.size() < 2) {
        outError = "read chain: the chain has fewer than two tokens";
        return false;
    }

    Jwt::Token token;
    if (!Jwt::parse(chain->mArray[1]->string(), token)) {
        outError = "read chain: parse jwt failed";
        return false;
    }

    std::unique_ptr<json::Value> claims = json::parse(token.mPayloadJson);
    const json::Value *extraData = claims != nullptr ? claims->get("extraData") : nullptr;
    const json::Value *identity = extraData != nullptr ? extraData->get("identity") : nullptr;

    if (identity == nullptr || identity->string().empty()) {
        outError = "read chain: no extra data found";
        return false;
    }

    const json::Value *displayName = extraData->get("displayName");
    const json::Value *xuid = extraData->get("XUID");
    const json::Value *titleId = extraData->get("titleId");

    outResult.mIdentity = identity->string();
    outResult.mDisplayName = displayName != nullptr ? displayName->string() : std::string();
    outResult.mXuid = xuid != nullptr ? xuid->string() : std::string();
    outResult.mTitleId = titleId != nullptr ? titleId->string() : std::string();
    return true;
}

bool MinecraftAuthentication::authenticate(const KeyPair &key, bool legacy, MinecraftAuthenticationResult &outResult,
                                           std::string &outError) {
    outResult = MinecraftAuthenticationResult();

    if (!legacy) {
        if (!requestMultiplayerToken(key, outResult.mMultiplayerToken, outError))
            return false;

        Jwt::Token token;
        if (!Jwt::parse(outResult.mMultiplayerToken, token)) {
            outError = "read multiplayer token: parse jwt failed";
            return false;
        }
        const auto claims = json::parse(token.mPayloadJson);
        const auto playFabId = claims != nullptr ? claims->get("mid") : nullptr;
        if (playFabId != nullptr && playFabId->isString())
            outResult.mPlayFabId = toLower(playFabId->string());
    }

    if (!requestChain(key, outResult.mChainJson, outError)) {
        outError = "request Minecraft auth chain: " + outError;
        return false;
    }

    return readChainIdentity(outResult.mChainJson, outResult, outError);
}
