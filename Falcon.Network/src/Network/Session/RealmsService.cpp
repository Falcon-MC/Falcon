#include "Network/Session/RealmsService.h"

#include "Core/Json/Json.h"
#include "Network/Auth/MinecraftAuthentication.h"
#include "Network/Http/HttpClient.h"
#include "Network/NetherNet/NetherNetJsonRpcSignaling.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <thread>

namespace {

    const char *REALMS_BASE_URL = "https://bedrock.frontendlegacy.realms.minecraft-services.net";
    const char *REALMS_RELYING_PARTY = "https://pocket.realms.minecraft.net/";
    const char *REALMS_USER_AGENT = "MCPE/UWP";
    const char *NETWORK_PROTOCOL_DEFAULT = "DEFAULT";
    const char *NETWORK_PROTOCOL_NETHERNET = "NETHERNET";
    const char *NETWORK_PROTOCOL_NETHERNET_JSON_RPC = "NETHERNET_JSONRPC";
    const int RETRY_INTERVAL_MS = 3000;
    const int RETRY_POLL_MS = 50;
    const size_t MAX_ERROR_BODY_PREVIEW = 512;
    const int REQUEST_ATTEMPTS = 3;
    const int RETRY_MIN_DELAY_MS = 500;
    const int RETRY_MAX_DELAY_MS = 8000;
    std::string lowercase(std::string value) {
        for (char &c: value)
            c = (char) std::tolower((unsigned char) c);
        return value;
    }

    bool retryable(int status) {
        return status == 429 || status == 408 || (status >= 500 && status != 503) || status >= 600;
    }

    std::string text(const json::Value *object, const char *key) {
        const json::Value *value = object != nullptr ? object->get(key) : nullptr;
        return value != nullptr ? value->string() : std::string();
    }

    bool flag(const json::Value *object, const char *key) {
        const json::Value *value = object != nullptr ? object->get(key) : nullptr;
        return value != nullptr && value->boolean();
    }

    double number(const json::Value *object, const char *key) {
        const json::Value *value = object != nullptr ? object->get(key) : nullptr;
        return value != nullptr ? value->number() : 0.0;
    }

    RealmPlayer readPlayer(const json::Value &player) {
        RealmPlayer result;
        result.mUuid = text(&player, "uuid");
        result.mName = text(&player, "Name");
        result.mPermission = text(&player, "permission");
        result.mOperator = flag(&player, "operator");
        result.mAccepted = flag(&player, "accepted");
        result.mOnline = flag(&player, "online");
        return result;
    }

    bool validPathSegment(const std::string &value) {
        if (value.empty() || value.size() > 128)
            return false;

        return std::all_of(value.begin(), value.end(), [](char c) {
            return std::isalnum((unsigned char) c) != 0 || c == '-' || c == '_';
        });
    }

    std::string trim(const std::string &value) {
        size_t start = 0;
        size_t end = value.size();

        while (start < end && std::isspace((unsigned char) value[start]))
            ++start;

        while (end > start && std::isspace((unsigned char) value[end - 1]))
            --end;

        return value.substr(start, end - start);
    }

    RealmDescription readRealm(const json::Value &server) {
        RealmDescription realm;
        realm.mId = (long long) number(&server, "id");
        realm.mName = text(&server, "name");
        realm.mOwner = text(&server, "owner");
        realm.mOwnerUuid = text(&server, "ownerUUID");
        realm.mMotd = text(&server, "motd");
        realm.mState = text(&server, "state");
        realm.mDefaultPermission = text(&server, "defaultPermission");
        realm.mWorldType = text(&server, "worldType");
        realm.mRemoteSubscriptionId = text(&server, "remoteSubscriptionID");
        realm.mExpired = flag(&server, "expired");
        realm.mExpiredTrial = flag(&server, "expiredTrial");
        realm.mGracePeriod = flag(&server, "gracePeriod");
        realm.mMember = flag(&server, "member");
        realm.mDaysLeft = (int) number(&server, "daysLeft");
        realm.mMaxPlayers = (int) number(&server, "maxPlayers");
        realm.mClubId = (int64_t) number(&server, "clubId");
        if (const json::Value *players = server.get("players"); players != nullptr && players->isArray()) {
            for (const std::unique_ptr<json::Value> &player: players->mArray) {
                if (player != nullptr && player->isObject())
                    realm.mPlayers.push_back(readPlayer(*player));
            }
        }
        return realm;
    }

    RealmInvite readInvite(const json::Value &invite) {
        RealmInvite result;
        result.mInvitationId = text(&invite, "invitationId");
        result.mWorldName = text(&invite, "worldName");
        result.mWorldDescription = text(&invite, "worldDescription");
        result.mOwnerName = text(&invite, "worldOwnerName");
        result.mOwnerUuid = text(&invite, "worldOwnerUuid");
        result.mDate = (int64_t) number(&invite, "date");
        return result;
    }

    std::string normalizeProtocol(const std::string &value) {
        size_t start = 0;
        size_t end = value.size();

        while (start < end && std::isspace((unsigned char) value[start]))
            ++start;

        while (end > start && std::isspace((unsigned char) value[end - 1]))
            --end;

        std::string normalized;
        normalized.reserve(end - start);

        for (size_t index = start; index < end; ++index)
            normalized.push_back((char) std::toupper((unsigned char) value[index]));

        return normalized;
    }

    std::string describeError(int status, const std::string &body) {
        std::string preview = body.substr(0, MAX_ERROR_BODY_PREVIEW);

        if (body.size() > MAX_ERROR_BODY_PREVIEW)
            preview += "...";

        return preview.empty() ? "HTTP Error: " + std::to_string(status)
                               : "HTTP Error: " + std::to_string(status) + ": " + preview;
    }

}

bool RealmAddress::toTarget(SessionConnectionTarget &outTarget, std::string &outError) const {
    const std::string protocol = normalizeProtocol(mNetworkProtocol);

    if (protocol == NETWORK_PROTOCOL_NETHERNET) {
        outTarget.mTransportLayer = TransportLayer::NetherNet;
        outTarget.mSignalingType = NetherNetSignalingType::WebSocket;
        outTarget.mNetworkId = mAddress;
        return true;
    }

    if (protocol == NETWORK_PROTOCOL_NETHERNET_JSON_RPC) {
        if (!nethernet::JsonRpcSignaling::isMessagingID(mAddress)) {
            outError = "realm address is not a player messaging UUID: " + mAddress;
            return false;
        }

        outTarget.mTransportLayer = TransportLayer::NetherNet;
        outTarget.mSignalingType = NetherNetSignalingType::JsonRpc;
        outTarget.mNetworkId = nethernet::JsonRpcSignaling::normalizeMessagingID(mAddress);
        return true;
    }

    if (protocol == NETWORK_PROTOCOL_DEFAULT || protocol.empty()) {
        const size_t colon = mAddress.rfind(':');

        if (colon == std::string::npos || colon == 0 || colon + 1 >= mAddress.size()) {
            outError = "realm address is not host:port: " + mAddress;
            return false;
        }

        const long port = strtol(mAddress.c_str() + colon + 1, nullptr, 10);
        if (port <= 0 || port > 65535) {
            outError = "realm address has an invalid port: " + mAddress;
            return false;
        }

        std::string host = mAddress.substr(0, colon);
        if (host.size() >= 2 && host.front() == '[' && host.back() == ']')
            host = host.substr(1, host.size() - 2);

        outTarget.mTransportLayer = TransportLayer::RakNet;
        outTarget.mHost = host;
        outTarget.mPort = (unsigned short) port;
        return true;
    }

    outError = "unknown realm network protocol: " + mNetworkProtocol;
    return false;
}

RealmsService::RealmsService(MinecraftAuthentication &authentication) : mAuthentication(authentication) {
}

bool RealmsService::_cancelled() const {
    return mCancel != nullptr && mCancel->load();
}

bool RealmsService::_wait(int milliseconds) const {
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    while (std::chrono::steady_clock::now() < until) {
        if (_cancelled())
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(std::min(milliseconds, 20)));
    }
    return !_cancelled();
}

/**
 * Sends one request to the Realms service, retrying network failures, rate
 * limits, timeouts and server errors with a growing delay that honours the
 * server's Retry-After. A mutation is only sent again after a rate limit,
 * which the service answers before acting, since any other failure leaves
 * unknown whether it took effect.
 */
bool RealmsService::_request(const std::string &method, const std::string &path, bool mutation, int &outStatus,
                             std::string &outBody, std::string &outError) {
    mLastError.clear();

    XboxLiveToken token;
    if (!mAuthentication.getXboxLiveAuthentication().requestToken(REALMS_RELYING_PARTY, token, outError)) {
        mLastError.mKind = _cancelled() ? ServiceErrorKind::Cancelled : ServiceErrorKind::Unauthorized;
        outError = "request realms token: " + outError;
        return false;
    }

    HttpClient::Headers headers;
    headers.emplace_back("User-Agent", REALMS_USER_AGENT);
    headers.emplace_back("Client-Version", mAuthentication.getGameVersion());
    headers.emplace_back("Authorization", token.getAuthorizationHeader());

    const std::string url = std::string(REALMS_BASE_URL) + path;
    int delay = RETRY_MIN_DELAY_MS;
    int retryAfter = 0;

    for (int attempt = 0; attempt < REQUEST_ATTEMPTS; ++attempt) {
        if (attempt > 0) {
            if (!_wait(std::max(delay, retryAfter))) {
                mLastError.mKind = ServiceErrorKind::Cancelled;
                outError = "realm request cancelled";
                return false;
            }
            delay = std::min(delay * 2, RETRY_MAX_DELAY_MS);
            retryAfter = 0;
        }

        struct Call {
            std::atomic<bool> done{false};
            bool sent = false;
            HttpResponse response;
            std::string error;
        };
        auto call = std::make_shared<Call>();
        std::thread([call, method, url, headers]() {
            call->sent = HttpClient::request(method, url, headers, std::string(), call->response, call->error);
            call->done = true;
        }).detach();

        while (!call->done) {
            if (!_wait(RETRY_POLL_MS)) {
                mLastError.mKind = ServiceErrorKind::Cancelled;
                outError = "realm request cancelled";
                return false;
            }
        }

        if (!call->sent) {
            outError = call->error;
            mLastError.mKind = ServiceError::classifyTransport(call->error, _cancelled());
            if (mutation)
                return false;
            continue;
        }

        HttpResponse &response = call->response;
        outStatus = response.mStatus;
        outBody = std::move(response.mBody);
        const std::string header = response.getHeader("Retry-After");
        retryAfter = ServiceError::parseRetryAfter(header, RETRY_MAX_DELAY_MS);
        mLastError.clear();

        const bool again = mutation ? outStatus == 429 : retryable(outStatus);
        if (!again || attempt + 1 == REQUEST_ATTEMPTS) {
            if (outStatus == 429)
                mLastError.mRetryAfterMs = ServiceError::parseRetryAfter(header, 3600 * 1000);
            return true;
        }
    }

    return false;
}

bool RealmsService::_fail(int status, const std::string &body, std::string &outError) {
    const int retryAfter = mLastError.mRetryAfterMs;
    mLastError.mKind = ServiceError::classify(status);
    mLastError.mStatus = status;
    mLastError.mRetryAfterMs = retryAfter;
    outError = describeError(status, body);
    return false;
}

bool RealmsService::requestRealms(std::vector<RealmDescription> &outRealms, std::string &outError) {
    outRealms.clear();

    int status = 0;
    std::string body;

    if (!_request("GET", "/worlds", false, status, body, outError))
        return false;

    if (status >= 400)
        return _fail(status, body, outError);

    std::unique_ptr<json::Value> root = json::parse(body);
    const json::Value *servers = root != nullptr ? root->get("servers") : nullptr;

    if (servers == nullptr || !servers->isArray())
        return true;

    for (const std::unique_ptr<json::Value> &server: servers->mArray) {
        if (server != nullptr && server->isObject())
            outRealms.push_back(readRealm(*server));
    }

    return true;
}

bool RealmsService::requestRealmByCode(const std::string &code, RealmDescription &outRealm, std::string &outError) {
    mLastError.clear();
    if (!isValidInviteCode(code)) {
        mLastError.mKind = ServiceErrorKind::NotFound;
        outError = "invalid realm invite code";
        return false;
    }

    int status = 0;
    std::string body;

    if (!_request("GET", "/worlds/v1/link/" + code, false, status, body, outError))
        return false;

    if (status == 404) {
        _fail(status, body, outError);
        outError = "realm not found";
        return false;
    }

    if (status >= 400)
        return _fail(status, body, outError);

    std::unique_ptr<json::Value> root = json::parse(body);
    if (root == nullptr || !root->isObject()) {
        mLastError.mKind = ServiceErrorKind::InvalidResponse;
        outError = "invalid realm response";
        return false;
    }

    outRealm = readRealm(*root);
    return true;
}

bool RealmsService::acceptInviteCode(const std::string &code, RealmDescription &outRealm, std::string &outError) {
    mLastError.clear();
    if (!isValidInviteCode(code)) {
        mLastError.mKind = ServiceErrorKind::NotFound;
        outError = "invalid realm invite code";
        return false;
    }

    int status = 0;
    std::string body;

    if (!_request("POST", "/invites/v1/link/accept/" + code, true, status, body, outError))
        return false;

    if (status >= 400)
        return _fail(status, body, outError);

    std::unique_ptr<json::Value> root = json::parse(body);
    if (root == nullptr || !root->isObject()) {
        mLastError.mKind = ServiceErrorKind::InvalidResponse;
        outError = "invalid realm response";
        return false;
    }

    outRealm = readRealm(*root);
    return true;
}

bool RealmsService::requestPendingInvites(std::vector<RealmInvite> &outInvites, std::string &outError) {
    outInvites.clear();

    int status = 0;
    std::string body;

    if (!_request("GET", "/invites/pending", false, status, body, outError))
        return false;

    if (status >= 400)
        return _fail(status, body, outError);

    std::unique_ptr<json::Value> root = json::parse(body);
    if (root == nullptr || !root->isObject()) {
        mLastError.mKind = ServiceErrorKind::InvalidResponse;
        outError = "invalid realm invites response";
        return false;
    }

    const json::Value *invites = root->get("invites");
    if (invites == nullptr || !invites->isArray())
        return true;

    for (const std::unique_ptr<json::Value> &invite: invites->mArray) {
        if (invite == nullptr || !invite->isObject())
            continue;

        RealmInvite parsed = readInvite(*invite);
        if (validPathSegment(parsed.mInvitationId))
            outInvites.push_back(std::move(parsed));
    }

    return true;
}

bool RealmsService::acceptInvite(const std::string &invitationId, std::string &outError) {
    mLastError.clear();
    if (!validPathSegment(invitationId)) {
        mLastError.mKind = ServiceErrorKind::NotFound;
        outError = "invalid realm invitation id";
        return false;
    }

    int status = 0;
    std::string body;

    if (!_request("PUT", "/invites/accept/" + invitationId, true, status, body, outError))
        return false;

    return status >= 400 ? _fail(status, body, outError) : true;
}

bool RealmsService::rejectInvite(const std::string &invitationId, std::string &outError) {
    mLastError.clear();
    if (!validPathSegment(invitationId)) {
        mLastError.mKind = ServiceErrorKind::NotFound;
        outError = "invalid realm invitation id";
        return false;
    }

    int status = 0;
    std::string body;

    if (!_request("PUT", "/invites/reject/" + invitationId, true, status, body, outError))
        return false;

    return status >= 400 ? _fail(status, body, outError) : true;
}

bool RealmsService::requestOnlinePlayers(long long realmId, std::vector<RealmPlayer> &outPlayers,
                                         std::string &outError) {
    outPlayers.clear();

    int status = 0;
    std::string body;

    if (!_request("GET", "/worlds/" + std::to_string(realmId), false, status, body, outError))
        return false;

    if (status == 404) {
        _fail(status, body, outError);
        outError = "realm not found";
        return false;
    }

    if (status == 403) {
        _fail(status, body, outError);
        outError = "player is not in the realm";
        return false;
    }

    if (status >= 400)
        return _fail(status, body, outError);

    std::unique_ptr<json::Value> root = json::parse(body);
    if (root == nullptr || !root->isObject()) {
        mLastError.mKind = ServiceErrorKind::InvalidResponse;
        outError = "invalid realm response";
        return false;
    }

    outPlayers = readRealm(*root).mPlayers;
    return true;
}

bool RealmsService::isValidInviteCode(const std::string &code) {
    return code.size() <= 64 && validPathSegment(code);
}

std::string RealmsService::inviteCode(const std::string &text) {
    const std::string trimmed = trim(text);
    const std::string lower = lowercase(trimmed);

    std::string rest;
    if (lower.rfind("realm/", 0) == 0) {
        rest = trimmed.substr(6);
        const size_t end = rest.find_first_of("?#");
        if (end != std::string::npos)
            rest.resize(end);
        return isValidInviteCode(rest) ? rest : std::string();
    }

    size_t start = 0;
    if (lower.rfind("https://", 0) == 0)
        start = 8;
    else if (lower.rfind("http://", 0) == 0)
        start = 7;

    const size_t hostEnd = trimmed.find_first_of("/?#", start);
    if (hostEnd == std::string::npos || trimmed[hostEnd] != '/')
        return {};

    const std::string host = lower.substr(start, hostEnd - start);
    if (host != "realms.gg" && host != "www.realms.gg")
        return {};

    std::string path = trimmed.substr(hostEnd + 1);
    const size_t queryStart = path.find_first_of("?#");
    if (queryStart != std::string::npos)
        path.resize(queryStart);

    if (!path.empty() && path.back() == '/')
        path.pop_back();

    return isValidInviteCode(path) ? path : std::string();
}

std::string RealmsService::parseInvite(const std::string &text, bool allowBareCode) {
    const std::string fromLink = inviteCode(text);
    if (!fromLink.empty() || !allowBareCode)
        return fromLink;

    const std::string trimmed = trim(text);
    return isValidInviteCode(trimmed) ? trimmed : std::string();
}

bool RealmsService::requestAddress(long long realmId, unsigned int timeoutMs, const std::atomic<bool> *cancel,
                                   RealmAddress &outAddress, std::string &outError) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    const std::string path = "/worlds/" + std::to_string(realmId) + "/join";

    for (;;) {
        int status = 0;
        std::string body;

        if (!_request("GET", path, false, status, body, outError))
            return false;

        if (status == 503) {
            const auto retryAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(RETRY_INTERVAL_MS);

            while (std::chrono::steady_clock::now() < retryAt) {
                if (cancel != nullptr && cancel->load()) {
                    mLastError.mKind = ServiceErrorKind::Cancelled;
                    outError = "realm join cancelled";
                    return false;
                }

                if (std::chrono::steady_clock::now() >= deadline) {
                    mLastError.mKind = ServiceErrorKind::Timeout;
                    outError = "timed out waiting for the realm to start";
                    return false;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_POLL_MS));
            }

            continue;
        }

        if (status == 404) {
            _fail(status, body, outError);
            outError = "realm not found";
            return false;
        }

        if (status == 403) {
            _fail(status, body, outError);
            outError = "player is not in the realm";
            return false;
        }

        if (status >= 400)
            return _fail(status, body, outError);

        std::unique_ptr<json::Value> root = json::parse(body);

        if (root == nullptr || !root->isObject()) {
            mLastError.mKind = ServiceErrorKind::InvalidResponse;
            outError = "invalid realm address response";
            return false;
        }

        const json::Value *address = root->get("address");
        const json::Value *protocol = root->get("networkProtocol");
        const json::Value *pendingUpdate = root->get("pendingUpdate");

        outAddress.mAddress = address != nullptr ? address->string() : std::string();
        outAddress.mNetworkProtocol = protocol != nullptr ? protocol->string() : std::string();
        outAddress.mPendingUpdate = pendingUpdate != nullptr && pendingUpdate->boolean();
        return true;
    }
}
