#include "Network/Session/XboxPresenceService.h"

#include "Network/Auth/MinecraftAuthentication.h"
#include "Network/Auth/XboxLiveAuthentication.h"
#include "Network/Http/HttpClient.h"

#include <cstdlib>

namespace {

    const char *XBOX_LIVE_RELYING_PARTY = "http://xboxlive.com";
    const char *PRESENCE_URL = "https://userpresence.xboxlive.com/users/xuid(";
    const char *PRESENCE_PATH = ")/devices/current/titles/current";
    const char *PRESENCE_CONTRACT = "3";
    const char *ACTIVE_STATE = "{\"state\":\"active\"}";

}

XboxPresenceService::XboxPresenceService(MinecraftAuthentication &authentication)
        : mAuthentication(authentication) {
}

bool XboxPresenceService::update(int &outHeartbeatSeconds, std::string &outError) {
    outHeartbeatSeconds = DEFAULT_HEARTBEAT_SECONDS;

    XboxLiveToken token;
    if (!mAuthentication.getXboxLiveAuthentication().requestToken(XBOX_LIVE_RELYING_PARTY, token, outError)) {
        outError = "request xbox live token: " + outError;
        return false;
    }
    if (token.mXuid.empty()) {
        outError = "the xbox live token has no xuid";
        return false;
    }

    HttpClient::Headers headers;
    headers.emplace_back("Authorization", token.getAuthorizationHeader());
    headers.emplace_back("x-xbl-contract-version", PRESENCE_CONTRACT);
    headers.emplace_back("Content-Type", "application/json");

    const std::string url = std::string(PRESENCE_URL) + token.mXuid + PRESENCE_PATH;
    HttpResponse response;
    if (!HttpClient::post(url, headers, ACTIVE_STATE, response, outError))
        return false;

    if (response.mStatus != 200 && response.mStatus != 201 && response.mStatus != 204) {
        outError = "POST " + url + ": status " + std::to_string(response.mStatus);
        return false;
    }

    const int heartbeat = std::atoi(response.getHeader("X-Heartbeat-After").c_str());
    if (heartbeat > 0)
        outHeartbeatSeconds = heartbeat;
    return true;
}
