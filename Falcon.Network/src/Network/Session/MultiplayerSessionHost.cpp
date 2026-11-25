#include "Network/Session/MultiplayerSessionHost.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "Network/Auth/AuthenticationUtils.h"
#include "Network/Auth/MinecraftAuthentication.h"
#include "Network/Crypto/Base64.h"
#include "Network/Http/HttpClient.h"
#include "Network/JsonText.h"

#include <cctype>
#include <chrono>
#include <memory>
#include <openssl/rand.h>

namespace {

    const char *MPSD_ENDPOINT = "https://sessiondirectory.xboxlive.com";
    const char *CONTRACT_VERSION = "107";
    const char *SERVICE_CONFIG_ID = "4fc10100-5f7a-4470-899b-280835760c07";
    const char *TEMPLATE_NAME = "MinecraftLobby";
    const char *XBOX_LIVE_RELYING_PARTY = "http://xboxlive.com";
    const char *CONNECTIONS_RESOURCE = "https://sessiondirectory.xboxlive.com/connections/";
    const char *CHANGE_TYPE_EVERYTHING = "everything";
    const char *RESTRICTION_FOLLOWED = "followed";
    const char *JOINABILITY_FRIENDS = "joinable_by_friends";
    const char *WORLD_TYPE = "Survival";
    const char *NIL_UUID = "00000000-0000-0000-0000-000000000000";
    const int BROADCAST_FRIENDS_OF_FRIENDS = 3;
    const int TRANSPORT_LAYER_NETHERNET = 2;
    const int CONNECTION_TYPE_WEBSOCKET = 3;
    const int CONNECTION_TYPE_JSON_RPC = 7;
    const int LEVEL_ID_BYTES = 8;
    const int MAINTENANCE_INTERVAL_MS = 1000;
    const int REFRESH_INTERVAL_SECONDS = 30;

    std::string toLower(const std::string &value) {
        std::string lowered;
        lowered.reserve(value.size());
        for (char character: value)
            lowered.push_back((char) std::tolower((unsigned char) character));
        return lowered;
    }

    std::string toUpper(const std::string &value) {
        std::string raised;
        raised.reserve(value.size());
        for (char character: value)
            raised.push_back((char) std::toupper((unsigned char) character));
        return raised;
    }

    HttpClient::Headers serviceHeaders(const std::string &authorization) {
        HttpClient::Headers headers;
        headers.emplace_back("Authorization", authorization);
        headers.emplace_back("x-xbl-contract-version", CONTRACT_VERSION);
        headers.emplace_back("Content-Type", "application/json");
        headers.emplace_back("Accept", "application/json");
        return headers;
    }

    std::string randomLevelId() {
        unsigned char raw[LEVEL_ID_BYTES];
        RAND_bytes(raw, sizeof(raw));
        return Base64::encode(std::string((const char *) raw, sizeof(raw)));
    }

}

MultiplayerSessionHost::MultiplayerSessionHost(MinecraftAuthentication &authentication)
        : mAuthentication(authentication), mSubscriptionId(0), mChanged(false), mWorldChanged(false),
          mPublished(false), mStopping(false) {
}

MultiplayerSessionHost::~MultiplayerSessionHost() {
    close();
}

bool MultiplayerSessionHost::_authorize(std::string &outError) {
    XboxLiveToken token;
    if (!mAuthentication.getXboxLiveAuthentication().requestToken(XBOX_LIVE_RELYING_PARTY, token, outError)) {
        outError = "request XSTS token: " + outError;
        return false;
    }

    if (token.mXuid.empty()) {
        outError = "authorization token does not claim XUID";
        return false;
    }

    mXuid = token.mXuid;
    mAuthorization = token.getAuthorizationHeader();
    return true;
}

bool MultiplayerSessionHost::_subscribe(std::string &outConnectionId, std::string &outError) {
    if (!mRta.isOpen()) {
        const nethernet::XboxRtaClient::EventHandler handler = [this](unsigned int subscriptionId,
                                                                      const std::string &custom) {
            _onEvent(subscriptionId, custom);
        };

        if (!mRta.connect(mAuthorization, handler)) {
            outError = "could not connect to the Xbox Live RTA service";
            return false;
        }
    }

    nethernet::RtaSubscription subscription;
    if (!mRta.subscribe(CONNECTIONS_RESOURCE, subscription)) {
        outError = std::string("subscribe to \"") + CONNECTIONS_RESOURCE + "\" failed";
        return false;
    }

    std::unique_ptr<json::Value> custom = json::parse(subscription.mCustom);
    const json::Value *connectionId = custom != nullptr ? custom->get("ConnectionId") : nullptr;

    if (connectionId == nullptr || connectionId->string().empty() || connectionId->string() == NIL_UUID) {
        outError = "missing RTA connection ID in subscription data";
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mSubscriptionId = subscription.mId;
    }

    outConnectionId = connectionId->string();
    return true;
}

std::string MultiplayerSessionHost::_customProperties() const {
    std::string connections;

    if (mWorld.mSignalingType == NetherNetSignalingType::JsonRpc) {
        connections = "{\"ConnectionType\":" + std::to_string(CONNECTION_TYPE_JSON_RPC) + ",\"HostIpAddress\":\"\","
                      "\"HostPort\":0,\"NetherNetId\":" + JsonText::quote(mWorld.mNetherNetId) +
                      ",\"PmsgId\":" + JsonText::quote(mWorld.mPlayerMessagingId) + "}";
    } else {
        connections = "{\"ConnectionType\":" + std::to_string(CONNECTION_TYPE_WEBSOCKET) + ",\"HostIpAddress\":\"\","
                      "\"HostPort\":0,\"NetherNetId\":" + JsonText::quote(mWorld.mNetherNetId) + "}";
    }

    std::string nonces = "{";
    bool first = true;
    for (const auto &entry: mNonces) {
        if (!first)
            nonces += ",";
        nonces += JsonText::quote(entry.first) + ":" + JsonText::quote(entry.second);
        first = false;
    }
    nonces += "}";

    return "{\"Joinability\":" + JsonText::quote(JOINABILITY_FRIENDS) +
           ",\"hostName\":" + JsonText::quote(mWorld.mHostName) +
           ",\"ownerId\":" + JsonText::quote(mXuid) +
           ",\"rakNetGUID\":\"\"" +
           ",\"version\":" + JsonText::quote(mWorld.mVersion) +
           ",\"levelId\":" + JsonText::quote(mLevelId) +
           ",\"worldName\":" + JsonText::quote(mWorld.mWorldName) +
           ",\"worldType\":" + JsonText::quote(WORLD_TYPE) +
           ",\"protocol\":" + std::to_string(mWorld.mProtocol) +
           ",\"MemberCount\":" + std::to_string(mWorld.mMemberCount) +
           ",\"MaxMemberCount\":" + std::to_string(mWorld.mMaxMemberCount) +
           ",\"BroadcastSetting\":" + std::to_string(BROADCAST_FRIENDS_OF_FRIENDS) +
           ",\"LanGame\":false,\"isEditorWorld\":false" +
           ",\"TransportLayer\":" + std::to_string(TRANSPORT_LAYER_NETHERNET) +
           ",\"OnlineCrossPlatformGame\":true,\"CrossPlayDisabled\":false,\"TitleId\":0" +
           ",\"SupportedConnections\":[" + connections + "]" +
           ",\"nonces\":" + nonces + "}";
}

bool MultiplayerSessionHost::_put(const std::string &body, bool create, std::string &outBody, bool &outDeleted,
                                  std::string &outError) {
    std::lock_guard<std::mutex> lock(mRequestMutex);

    if (!_authorize(outError))
        return false;

    HttpClient::Headers headers = serviceHeaders(mAuthorization);
    headers.emplace_back(create ? "If-None-Match" : "If-Match", "*");

    HttpResponse response;
    if (!HttpClient::request("PUT", mSessionUrl, headers, body, response, outError))
        return false;

    outDeleted = response.mStatus == 204;
    const int expected = create ? 201 : 200;

    if (response.mStatus != expected && !outDeleted) {
        outError = "PUT " + mSessionUrl + ": status " + std::to_string(response.mStatus);
        return false;
    }

    outBody = response.mBody;
    return true;
}

bool MultiplayerSessionHost::_createSession(const std::string &connectionId, std::string &outError) {
    std::string custom;

    {
        std::lock_guard<std::mutex> lock(mMutex);
        custom = _customProperties();
    }

    const std::string body = "{\"properties\":{\"system\":{\"joinRestriction\":" + JsonText::quote(RESTRICTION_FOLLOWED) +
                             ",\"readRestriction\":" + JsonText::quote(RESTRICTION_FOLLOWED) + "},\"custom\":" + custom +
                             "},\"members\":{\"me\":{\"constants\":{\"system\":{\"initialize\":true,\"xuid\":" +
                             JsonText::quote(mXuid) + "}},\"properties\":{\"system\":{\"active\":true,\"connection\":" +
                             JsonText::quote(connectionId) + ",\"subscription\":{\"id\":" +
                             JsonText::quote(toUpper(AuthenticationUtils::generateUuid())) + ",\"changeTypes\":[" +
                             JsonText::quote(CHANGE_TYPE_EVERYTHING) + "]}}}}}}";

    std::string response;
    bool deleted = false;
    if (!_put(body, true, response, deleted, outError))
        return false;

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mPublishedCustom = custom;
        mConnectionId = connectionId;
    }

    return _writeActivity(outError);
}

bool MultiplayerSessionHost::_writeActivity(std::string &outError) {
    const std::string url = std::string(MPSD_ENDPOINT) + "/handles";
    const std::string body = "{\"type\":\"activity\",\"sessionRef\":{\"scid\":" + JsonText::quote(SERVICE_CONFIG_ID) +
                             ",\"templateName\":" + JsonText::quote(TEMPLATE_NAME) + ",\"name\":" +
                             JsonText::quote(mSessionName) + "},\"version\":1}";

    std::lock_guard<std::mutex> lock(mRequestMutex);

    HttpResponse response;
    if (!HttpClient::post(url, serviceHeaders(mAuthorization), body, response, outError))
        return false;

    if (response.mStatus != 200 && response.mStatus != 201) {
        outError = "POST " + url + ": status " + std::to_string(response.mStatus);
        return false;
    }

    return true;
}

bool MultiplayerSessionHost::publish(const HostedWorld &world, std::string &outError) {
    if (mPublished.load()) {
        outError = "a multiplayer session is already published";
        return false;
    }

    if (!_authorize(outError))
        return false;

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mWorld = world;
        mNonces.clear();
        mLevelId = randomLevelId();
        mSessionName = toUpper(AuthenticationUtils::generateUuid());
        mSessionUrl = std::string(MPSD_ENDPOINT) + "/serviceconfigs/" + SERVICE_CONFIG_ID + "/sessionTemplates/" +
                      TEMPLATE_NAME + "/sessions/" + mSessionName;
        mSessionReference = toLower(std::string(SERVICE_CONFIG_ID) + "~" + TEMPLATE_NAME + "~" + mSessionName);
        mChanged = false;
        mWorldChanged = false;
    }

    std::string connectionId;
    if (!_subscribe(connectionId, outError) || !_createSession(connectionId, outError)) {
        mRta.close();
        return false;
    }

    mStopping.store(false);
    mPublished.store(true);
    mThread = std::thread(&MultiplayerSessionHost::_maintain, this);

    LOG_INFO(LogAreaID::Network, "Published the multiplayer session %s", mSessionName.c_str());
    return true;
}

void MultiplayerSessionHost::update(const HostedWorld &world) {
    {
        std::lock_guard<std::mutex> lock(mMutex);

        if (world.mWorldName == mWorld.mWorldName && world.mHostName == mWorld.mHostName &&
            world.mMemberCount == mWorld.mMemberCount && world.mMaxMemberCount == mWorld.mMaxMemberCount &&
            world.mNetherNetId == mWorld.mNetherNetId && world.mPlayerMessagingId == mWorld.mPlayerMessagingId)
            return;

        mWorld = world;
        mWorldChanged = true;
    }

    mSignal.notify_all();
}

bool MultiplayerSessionHost::validateNonce(const std::string &xuid, const std::string &nonce) const {
    std::lock_guard<std::mutex> lock(mMutex);

    const auto it = mNonces.find(xuid);
    return it != mNonces.end() && !nonce.empty() && it->second == nonce;
}

void MultiplayerSessionHost::_onEvent(unsigned int subscriptionId, const std::string &custom) {
    std::unique_ptr<json::Value> event = json::parse(custom);
    const json::Value *taps = event != nullptr ? event->get("shoulderTaps") : nullptr;
    if (taps == nullptr || !taps->isArray())
        return;

    bool matched = false;

    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (subscriptionId != mSubscriptionId)
            return;

        for (const std::unique_ptr<json::Value> &tap: taps->mArray) {
            const json::Value *resource = tap != nullptr ? tap->get("resource") : nullptr;
            if (resource != nullptr && toLower(resource->string()) == mSessionReference)
                matched = true;
        }

        if (matched)
            mChanged = true;
    }

    if (matched)
        mSignal.notify_all();
}

bool MultiplayerSessionHost::_sync(std::string &outError) {
    std::string body;

    {
        std::lock_guard<std::mutex> lock(mRequestMutex);

        if (!_authorize(outError))
            return false;

        HttpResponse response;
        if (!HttpClient::get(mSessionUrl, serviceHeaders(mAuthorization), response, outError))
            return false;

        if (response.mStatus == 204 || response.mStatus == 404) {
            outError = "the multiplayer session was deleted";
            return false;
        }

        if (response.mStatus != 200) {
            outError = "GET " + mSessionUrl + ": status " + std::to_string(response.mStatus);
            return false;
        }

        body = response.mBody;
    }

    _assignNonces(body);
    return true;
}

void MultiplayerSessionHost::_assignNonces(const std::string &sessionBody) {
    std::unique_ptr<json::Value> session = json::parse(sessionBody);
    const json::Value *members = session != nullptr ? session->get("members") : nullptr;
    if (members == nullptr || !members->isObject())
        return;

    std::lock_guard<std::mutex> lock(mMutex);
    std::map<std::string, std::string> nonces;

    for (const auto &entry: members->mObject) {
        const json::Value *constants = entry.second != nullptr ? entry.second->get("constants") : nullptr;
        const json::Value *system = constants != nullptr ? constants->get("system") : nullptr;
        const json::Value *xuid = system != nullptr ? system->get("xuid") : nullptr;
        if (xuid == nullptr || xuid->string().empty() || xuid->string() == mXuid)
            continue;

        const auto existing = mNonces.find(xuid->string());
        nonces[xuid->string()] = existing != mNonces.end() ? existing->second
                                                           : toUpper(AuthenticationUtils::generateUuid());
    }

    if (nonces != mNonces) {
        mNonces = std::move(nonces);
        mWorldChanged = true;
    }
}

bool MultiplayerSessionHost::_publishCustom(std::string &outError) {
    std::string custom;

    {
        std::lock_guard<std::mutex> lock(mMutex);
        custom = _customProperties();
        mWorldChanged = false;

        if (custom == mPublishedCustom)
            return true;
    }

    std::string response;
    bool deleted = false;
    if (!_put("{\"properties\":{\"custom\":" + custom + "}}", false, response, deleted, outError))
        return false;

    if (deleted) {
        outError = "the multiplayer session was deleted";
        return false;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    mPublishedCustom = custom;
    return true;
}

bool MultiplayerSessionHost::_updateConnection(const std::string &connectionId, std::string &outError) {
    const std::string body = "{\"members\":{\"me\":{\"properties\":{\"system\":{\"active\":true,\"connection\":" +
                             JsonText::quote(connectionId) + "}}}}}";

    std::string response;
    bool deleted = false;
    if (!_put(body, false, response, deleted, outError))
        return false;

    if (deleted) {
        outError = "the multiplayer session was deleted";
        return false;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    mConnectionId = connectionId;
    return true;
}

bool MultiplayerSessionHost::_recover(std::string &outError) {
    if (_restore(outError)) {
        mFailedRecoveries.store(0);
        return true;
    }
    mFailedRecoveries.fetch_add(1);
    return false;
}

bool MultiplayerSessionHost::_restore(std::string &outError) {
    mRta.close();

    std::string connectionId;
    if (!_subscribe(connectionId, outError))
        return false;

    std::string error;
    if (_updateConnection(connectionId, error))
        return true;

    LOG_WARN(LogAreaID::Network, "Republishing the multiplayer session: %s", error.c_str());

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mSessionName = toUpper(AuthenticationUtils::generateUuid());
        mSessionUrl = std::string(MPSD_ENDPOINT) + "/serviceconfigs/" + SERVICE_CONFIG_ID + "/sessionTemplates/" +
                      TEMPLATE_NAME + "/sessions/" + mSessionName;
        mSessionReference = toLower(std::string(SERVICE_CONFIG_ID) + "~" + TEMPLATE_NAME + "~" + mSessionName);
        mNonces.clear();
    }

    return _createSession(connectionId, outError);
}

void MultiplayerSessionHost::_maintain() {
    auto lastRefresh = std::chrono::steady_clock::now();

    while (!mStopping.load()) {
        bool changed;
        bool worldChanged;

        {
            std::unique_lock<std::mutex> lock(mMutex);
            mSignal.wait_for(lock, std::chrono::milliseconds(MAINTENANCE_INTERVAL_MS), [this]() {
                return mChanged || mWorldChanged || mStopping.load();
            });

            changed = mChanged;
            worldChanged = mWorldChanged;
            mChanged = false;
        }

        if (mStopping.load())
            break;

        std::string error;

        if (!mRta.isOpen()) {
            if (!_recover(error))
                LOG_WARN(LogAreaID::Network, "Could not restore the multiplayer session: %s", error.c_str());
            continue;
        }

        if (changed && !_sync(error)) {
            LOG_WARN(LogAreaID::Network, "Could not read the multiplayer session: %s", error.c_str());
            if (!_recover(error))
                LOG_WARN(LogAreaID::Network, "Could not restore the multiplayer session: %s", error.c_str());
            continue;
        }

        const auto now = std::chrono::steady_clock::now();
        const bool refresh = std::chrono::duration_cast<std::chrono::seconds>(now - lastRefresh).count()
                             >= REFRESH_INTERVAL_SECONDS;

        {
            std::lock_guard<std::mutex> lock(mMutex);
            worldChanged = worldChanged || mWorldChanged;
        }

        if (!worldChanged && !refresh)
            continue;

        lastRefresh = now;
        if (!_publishCustom(error)) {
            LOG_WARN(LogAreaID::Network, "Could not update the multiplayer session: %s", error.c_str());
            if (!_recover(error))
                LOG_WARN(LogAreaID::Network, "Could not restore the multiplayer session: %s", error.c_str());
        }
    }
}

void MultiplayerSessionHost::close() {
    if (!mPublished.exchange(false))
        return;

    mStopping.store(true);
    mSignal.notify_all();
    if (mThread.joinable())
        mThread.join();

    std::string response;
    std::string error;
    bool deleted = false;
    if (!_put("{\"members\":{\"me\":null}}", false, response, deleted, error))
        LOG_WARN(LogAreaID::Network, "Could not close the multiplayer session: %s", error.c_str());

    mRta.close();
}
