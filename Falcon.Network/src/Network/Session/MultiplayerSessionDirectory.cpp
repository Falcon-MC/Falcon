#include "Network/Session/MultiplayerSessionDirectory.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "Network/Auth/AuthenticationUtils.h"
#include "Network/Auth/MinecraftAuthentication.h"
#include "Network/Http/HttpClient.h"
#include "Network/JsonText.h"
#include "Network/NetherNet/NetherNetJsonRpcSignaling.h"

#include <cctype>
#include <chrono>
#include <memory>

namespace {

    const char *MPSD_ENDPOINT = "https://sessiondirectory.xboxlive.com";
    const char *CONTRACT_VERSION = "107";
    const char *SERVICE_CONFIG_ID = "4fc10100-5f7a-4470-899b-280835760c07";
    const char *XBOX_LIVE_RELYING_PARTY = "http://xboxlive.com";
    const char *CONNECTIONS_RESOURCE = "https://sessiondirectory.xboxlive.com/connections/";
    const char *CHANGE_TYPE_EVERYTHING = "everything";
    const char *NIL_UUID = "00000000-0000-0000-0000-000000000000";
    const int SESSION_POLL_MS = 100;

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

    bool isNetworkId(const std::string &value) {
        if (value.empty() || value.size() > 20)
            return false;

        unsigned long long parsed = 0;

        for (char character: value) {
            if (character < '0' || character > '9')
                return false;

            const unsigned long long digit = (unsigned long long) (character - '0');

            if (parsed > (0xffffffffffffffffull - digit) / 10)
                return false;

            parsed = parsed * 10 + digit;
        }

        return true;
    }

    bool validateNetherNetId(const std::string &value, std::string &outError) {
        if (isNetworkId(value) || nethernet::JsonRpcSignaling::isMessagingID(value))
            return true;

        outError = "NetherNetID \"" + value + "\" is neither a uint64 nor a UUID";
        return false;
    }

    std::string readString(const json::Value &object, const std::string &key) {
        const json::Value *value = JsonText::getMember(object, key, true);
        return value != nullptr ? value->string() : std::string();
    }

    int readInteger(const json::Value &object, const std::string &key) {
        const json::Value *value = JsonText::getMember(object, key, true);
        return value != nullptr ? value->integer() : 0;
    }

    HttpClient::Headers serviceHeaders(const std::string &authorization, bool hasBody) {
        HttpClient::Headers headers;
        headers.emplace_back("Authorization", authorization);
        headers.emplace_back("x-xbl-contract-version", CONTRACT_VERSION);

        if (hasBody)
            headers.emplace_back("Content-Type", "application/json");

        return headers;
    }

    bool parseSessionLocation(const std::string &location, std::string &outScid, std::string &outTemplate,
                              std::string &outName) {
        std::string path = location;

        const size_t scheme = path.find("://");
        if (scheme != std::string::npos) {
            const size_t slash = path.find('/', scheme + 3);
            path = slash == std::string::npos ? std::string() : path.substr(slash);
        }

        const size_t query = path.find_first_of("?#");
        if (query != std::string::npos)
            path = path.substr(0, query);

        std::vector<std::string> segments;
        size_t start = path.empty() || path[0] != '/' ? 0 : 1;

        for (;;) {
            const size_t end = path.find('/', start);
            segments.push_back(path.substr(start, end == std::string::npos ? std::string::npos : end - start));

            if (end == std::string::npos)
                break;

            start = end + 1;
        }

        if (segments.size() != 6 || toLower(segments[0]) != "serviceconfigs" ||
            toLower(segments[2]) != "sessiontemplates" || toLower(segments[4]) != "sessions")
            return false;

        if (!nethernet::JsonRpcSignaling::isMessagingID(segments[1]) || segments[3].empty() || segments[5].empty())
            return false;

        outScid = segments[1];
        outTemplate = segments[3];
        outName = segments[5];
        return true;
    }

}

bool MultiplayerConnection::validate(std::string &outError) const {
    switch (mType) {
        case TYPE_SIGNALING_OVER_JSON_RPC:
            if (!validateNetherNetId(mNetherNetId, outError))
                return false;

            if (mPlayerMessagingId.empty() || mPlayerMessagingId == NIL_UUID) {
                outError = "Connection.PlayerMessagingID is nil";
                return false;
            }

            return true;

        case TYPE_SIGNALING_OVER_WEBSOCKET:
            return validateNetherNetId(mNetherNetId, outError);

        default:
            outError = "invalid Connection.Type: " + std::to_string(mType);
            return false;
    }
}

bool MultiplayerWorld::parse(const std::string &customProperties, MultiplayerWorld &outWorld, std::string &outError) {
    std::unique_ptr<json::Value> root = json::parse(customProperties);

    if (root == nullptr || !root->isObject()) {
        outError = "decode custom properties: not a JSON object";
        return false;
    }

    MultiplayerWorld world;
    world.mHostName = readString(*root, "hostName");
    world.mOwnerId = readString(*root, "ownerId");
    world.mVersion = readString(*root, "version");
    world.mWorldName = readString(*root, "worldName");
    world.mProtocol = readInteger(*root, "protocol");
    world.mMemberCount = readInteger(*root, "MemberCount");
    world.mMaxMemberCount = readInteger(*root, "MaxMemberCount");
    world.mTransportLayer = readInteger(*root, "TransportLayer");

    const json::Value *realmId = JsonText::getMember(*root, "RealmId", true);
    world.mRealmId = realmId != nullptr ? (long long) realmId->number() : 0;

    std::string rawConnections;
    std::vector<std::string> connections;

    if (JsonText::findMember(customProperties, "SupportedConnections", true, rawConnections) &&
        rawConnections != "null") {
        if (!JsonText::splitArray(rawConnections, connections)) {
            outError = "decode custom properties: SupportedConnections is not an array";
            return false;
        }
    }

    for (const std::string &rawConnection: connections) {
        std::unique_ptr<json::Value> entry = json::parse(rawConnection);

        if (entry == nullptr || !entry->isObject()) {
            outError = "decode custom properties: a supported connection is not an object";
            return false;
        }

        MultiplayerConnection connection;
        connection.mType = readInteger(*entry, "ConnectionType");
        connection.mHostIpAddress = readString(*entry, "HostIpAddress");
        connection.mHostPort = (unsigned short) readInteger(*entry, "HostPort");
        connection.mPlayerMessagingId = nethernet::JsonRpcSignaling::normalizeMessagingID(
                readString(*entry, "PmsgId"));

        std::string rawId;
        if (JsonText::findMember(rawConnection, "NetherNetId", true, rawId) && rawId != "null") {
            if (!rawId.empty() && rawId[0] == '"') {
                if (!JsonText::readString(rawId, connection.mNetherNetId)) {
                    outError = "decode custom properties: invalid NetherNetId";
                    return false;
                }
            } else {
                connection.mNetherNetId = rawId;
            }
        }

        world.mSupportedConnections.push_back(connection);
    }

    const json::Value *nonces = JsonText::getMember(*root, "nonces", true);

    if (nonces != nullptr && nonces->isObject()) {
        for (const auto &entry: nonces->mObject) {
            if (entry.second != nullptr && entry.second->isString())
                world.mNonces[entry.first] = entry.second->mString;
        }
    }

    outWorld = std::move(world);
    return true;
}

bool MultiplayerWorld::selectConnection(SessionConnectionTarget &outTarget, std::string &outError) const {
    if (mTransportLayer != TRANSPORT_LAYER_NETHERNET) {
        outError = "invalid transport layer: " + std::to_string(mTransportLayer);
        return false;
    }

    std::string errors;

    for (const MultiplayerConnection &connection: mSupportedConnections) {
        std::string error;

        if (!connection.validate(error)) {
            errors += errors.empty() ? error : "; " + error;
            continue;
        }

        outTarget.mTransportLayer = TransportLayer::NetherNet;

        if (connection.mType == MultiplayerConnection::TYPE_SIGNALING_OVER_JSON_RPC) {
            outTarget.mSignalingType = NetherNetSignalingType::JsonRpc;
            outTarget.mNetworkId = connection.mPlayerMessagingId;
        } else {
            outTarget.mSignalingType = NetherNetSignalingType::WebSocket;
            outTarget.mNetworkId = connection.mNetherNetId;
        }

        return true;
    }

    outError = errors.empty() ? "no supported signaling connection" : errors;
    return false;
}

MultiplayerSessionDirectory::MultiplayerSessionDirectory(MinecraftAuthentication &authentication)
        : mAuthentication(authentication), mHasConnection(false), mJoined(false), mSubscriptionId(0),
          mSubscribed(false), mChanged(false) {
}

MultiplayerSessionDirectory::~MultiplayerSessionDirectory() {
    std::string error;

    if (!leave(error))
        LOG_WARN(LogAreaID::Network, "Could not leave the multiplayer session: %s", error.c_str());

    mRta.close();
}

bool MultiplayerSessionDirectory::isJoined() const {
    return mJoined;
}

const MultiplayerWorld &MultiplayerSessionDirectory::getWorld() const {
    return mWorld;
}

bool MultiplayerSessionDirectory::_authorize(XboxLiveToken &outToken, std::string &outError) {
    if (!mAuthentication.getXboxLiveAuthentication().requestToken(XBOX_LIVE_RELYING_PARTY, outToken, outError)) {
        outError = "request XSTS token: " + outError;
        return false;
    }

    if (outToken.mXuid.empty()) {
        outError = "authorization token does not claim XUID";
        return false;
    }

    mXuid = outToken.mXuid;
    mAuthorization = outToken.getAuthorizationHeader();
    return true;
}

bool MultiplayerSessionDirectory::queryWorlds(std::vector<MultiplayerWorld> &outWorlds, std::string &outError) {
    outWorlds.clear();

    XboxLiveToken token;
    if (!_authorize(token, outError))
        return false;

    const std::string url = std::string(MPSD_ENDPOINT) + "/handles/query?include=relatedInfo,customProperties";
    const std::string body = "{\"type\":\"activity\",\"scid\":" + JsonText::quote(SERVICE_CONFIG_ID) +
                             ",\"owners\":{\"people\":{\"moniker\":\"people\",\"monikerXuid\":" +
                             JsonText::quote(mXuid) + "}}}";

    HttpResponse response;
    if (!HttpClient::post(url, serviceHeaders(mAuthorization, true), body, response, outError))
        return false;

    if (response.mStatus != 200 && response.mStatus != 201) {
        outError = "POST " + url + ": status " + std::to_string(response.mStatus);
        return false;
    }

    std::string rawResults;
    std::vector<std::string> activities;

    if (!JsonText::findMember(response.mBody, "results", false, rawResults) || rawResults == "null")
        return true;

    if (!JsonText::splitArray(rawResults, activities)) {
        outError = "decode activities: results is not an array";
        return false;
    }

    for (const std::string &activity: activities) {
        std::unique_ptr<json::Value> handle = json::parse(activity);
        if (handle == nullptr || !handle->isObject())
            continue;

        const json::Value *relatedInfo = handle->get("relatedInfo");
        if (relatedInfo == nullptr || !relatedInfo->isObject())
            continue;

        const json::Value *closed = relatedInfo->get("closed");
        if (closed != nullptr && closed->boolean())
            continue;

        std::string customProperties;
        if (!JsonText::findMember(activity, "customProperties", false, customProperties))
            continue;

        MultiplayerWorld world;
        std::string error;

        if (!MultiplayerWorld::parse(customProperties, world, error)) {
            LOG_WARN(LogAreaID::Network, "Error decoding world data: %s", error.c_str());
            continue;
        }

        const json::Value *id = handle->get("id");
        const json::Value *ownerXuid = handle->get("ownerXuid");

        world.mHandleId = id != nullptr ? id->string() : std::string();
        world.mOwnerXuid = ownerXuid != nullptr ? ownerXuid->string() : std::string();
        outWorlds.push_back(std::move(world));
    }

    return true;
}

bool MultiplayerSessionDirectory::_subscribe(const XboxLiveToken &token, std::string &outConnectionId,
                                             std::string &outError) {
    if (!mRta.isOpen()) {
        const nethernet::XboxRtaClient::EventHandler handler = [this](unsigned int subscriptionId,
                                                                      const std::string &custom) {
            _onEvent(subscriptionId, custom);
        };

        if (!mRta.connect(token.getAuthorizationHeader(), handler)) {
            outError = "could not connect to the Xbox Live RTA service";
            return false;
        }
    }

    nethernet::RtaSubscription subscription;
    if (!mRta.subscribe(CONNECTIONS_RESOURCE, subscription)) {
        outError = std::string("subscribe to \"") + CONNECTIONS_RESOURCE + "\" failed";
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mSubscriptionId = subscription.mId;
        mSubscribed = true;
    }

    std::unique_ptr<json::Value> custom = json::parse(subscription.mCustom);
    const json::Value *connectionId = custom != nullptr ? custom->get("ConnectionId") : nullptr;

    if (connectionId == nullptr || connectionId->string().empty() || connectionId->string() == NIL_UUID) {
        outError = "missing RTA connection ID in subscription data";
        return false;
    }

    outConnectionId = connectionId->string();
    return true;
}

void MultiplayerSessionDirectory::_onEvent(unsigned int subscriptionId, const std::string &custom) {
    std::unique_ptr<json::Value> event = json::parse(custom);
    const json::Value *taps = event != nullptr ? event->get("shoulderTaps") : nullptr;

    if (taps == nullptr || !taps->isArray())
        return;

    bool matched = false;

    {
        std::lock_guard<std::mutex> lock(mMutex);

        if (!mSubscribed || subscriptionId != mSubscriptionId || mSessionReference.empty())
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

bool MultiplayerSessionDirectory::_applySession(const std::string &body, bool &outReady, std::string &outError) {
    std::string properties;
    std::string custom;

    if (!JsonText::findMember(body, "properties", false, properties) ||
        !JsonText::findMember(properties, "custom", false, custom)) {
        outError = "the multiplayer session has no custom properties";
        return false;
    }

    MultiplayerWorld world;
    if (!MultiplayerWorld::parse(custom, world, outError))
        return false;

    world.mHandleId = mWorld.mHandleId;
    world.mOwnerXuid = mWorld.mOwnerXuid;

    SessionConnectionTarget target;
    std::string connectionError;

    if (world.selectConnection(target, connectionError)) {
        target.mNonce = mTarget.mNonce;
        mTarget = target;
        mHasConnection = true;
    } else if (!mHasConnection) {
        outError = "select connection method: " + connectionError;
        return false;
    }

    mWorld = std::move(world);

    if (mTarget.mNonce.empty()) {
        const auto it = mWorld.mNonces.find(mXuid);

        if (it != mWorld.mNonces.end()) {
            if (it->second.empty()) {
                outError = "host published empty nonce for caller";
                return false;
            }

            mTarget.mNonce = it->second;
        }
    }

    outReady = !mTarget.mNonce.empty();
    return true;
}

bool MultiplayerSessionDirectory::_sync(std::string &outError) {
    HttpClient::Headers headers = serviceHeaders(mAuthorization, false);
    headers.emplace_back("Accept", "application/json");
    headers.emplace_back("If-None-Match", mEtag);

    HttpResponse response;
    if (!HttpClient::get(mSessionUrl, headers, response, outError))
        return false;

    if (response.mStatus == 304)
        return true;

    if (response.mStatus != 200) {
        outError = "GET " + mSessionUrl + ": status " + std::to_string(response.mStatus);
        return false;
    }

    const std::string etag = response.getHeader("etag");
    if (!etag.empty())
        mEtag = etag;

    bool ready = false;
    return _applySession(response.mBody, ready, outError);
}

bool MultiplayerSessionDirectory::join(const std::string &handleId, unsigned int timeoutMs,
                                       const std::atomic<bool> *cancel, SessionConnectionTarget &outTarget,
                                       std::string &outError) {
    if (mJoined) {
        outError = "a multiplayer session is already joined";
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    XboxLiveToken token;
    if (!_authorize(token, outError))
        return false;

    std::string connectionId;
    if (!_subscribe(token, connectionId, outError)) {
        _release();
        return false;
    }

    const std::string body = "{\"members\":{\"me\":{\"constants\":{\"system\":{\"xuid\":" + JsonText::quote(mXuid) +
                             ",\"initialize\":true}},\"properties\":{\"system\":{\"active\":true,\"connection\":" +
                             JsonText::quote(connectionId) + ",\"subscription\":{\"id\":" +
                             JsonText::quote(toUpper(AuthenticationUtils::generateUuid())) +
                             ",\"changeTypes\":[" + JsonText::quote(CHANGE_TYPE_EVERYTHING) + "]}}}}}}";

    const std::string url = std::string(MPSD_ENDPOINT) + "/handles/" + handleId + "/session";

    HttpClient::Headers headers = serviceHeaders(mAuthorization, true);
    headers.emplace_back("If-Match", "*");

    HttpResponse response;
    if (!HttpClient::request("PUT", url, headers, body, response, outError)) {
        _release();
        return false;
    }

    if (response.mStatus != 200) {
        outError = "PUT " + url + ": status " + std::to_string(response.mStatus);
        _release();
        return false;
    }

    std::string scid;
    std::string templateName;
    std::string name;

    const std::string location = response.getHeader("content-location");
    if (location.empty() || !parseSessionLocation(location, scid, templateName, name)) {
        outError = "parse session reference from Content-Location header: " + location;
        _release();
        return false;
    }

    mSessionUrl = std::string(MPSD_ENDPOINT) + "/serviceconfigs/" + scid + "/sessionTemplates/" + templateName +
                  "/sessions/" + name;
    mEtag = response.getHeader("etag");
    mJoined = true;
    mHasConnection = false;
    mTarget = SessionConnectionTarget();
    mWorld = MultiplayerWorld();
    mWorld.mHandleId = handleId;

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mSessionReference = toLower(scid + "~" + templateName + "~" + name);
        mChanged = false;
    }

    bool ready = false;
    std::string leaveError;

    if (!_applySession(response.mBody, ready, outError)) {
        leave(leaveError);
        return false;
    }

    while (!ready) {
        if (cancel != nullptr && cancel->load()) {
            outError = "join cancelled";
            leave(leaveError);
            return false;
        }

        if (std::chrono::steady_clock::now() >= deadline) {
            outError = "timed out waiting for the host to publish a connection and a nonce";
            leave(leaveError);
            return false;
        }

        if (!mRta.isOpen()) {
            outError = "subscription lost";
            leave(leaveError);
            return false;
        }

        bool changed;

        {
            std::unique_lock<std::mutex> lock(mMutex);
            mSignal.wait_for(lock, std::chrono::milliseconds(SESSION_POLL_MS), [this]() {
                return mChanged;
            });

            changed = mChanged;
            mChanged = false;
        }

        if (!changed)
            continue;

        if (!_sync(outError)) {
            leave(leaveError);
            return false;
        }

        ready = !mTarget.mNonce.empty();
    }

    outTarget = mTarget;
    return true;
}

bool MultiplayerSessionDirectory::leave(std::string &outError) {
    if (!mJoined) {
        _release();
        return true;
    }

    HttpClient::Headers headers = serviceHeaders(mAuthorization, true);
    headers.emplace_back("If-Match", "*");

    HttpResponse response;
    if (!HttpClient::request("PUT", mSessionUrl, headers, "{\"members\":{\"me\":null}}", response, outError))
        return false;

    if (response.mStatus != 200 && response.mStatus != 204) {
        outError = "PUT " + mSessionUrl + ": status " + std::to_string(response.mStatus);
        return false;
    }

    mJoined = false;
    _release();
    return true;
}

void MultiplayerSessionDirectory::_release() {
    unsigned int subscriptionId = 0;
    bool subscribed;

    {
        std::lock_guard<std::mutex> lock(mMutex);
        subscribed = mSubscribed;
        subscriptionId = mSubscriptionId;
        mSubscribed = false;
        mSessionReference.clear();
        mChanged = false;
    }

    if (subscribed && mRta.isOpen())
        mRta.unsubscribe(subscriptionId);
}
