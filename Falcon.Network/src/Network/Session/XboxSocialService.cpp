#include "Network/Session/XboxSocialService.h"

#include "Core/Json/Json.h"
#include "Network/Auth/MinecraftAuthentication.h"
#include "Network/Auth/XboxLiveAuthentication.h"
#include "Network/Http/HttpClient.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <memory>
#include <thread>

namespace {

    const char *XBOX_LIVE_RELYING_PARTY = "http://xboxlive.com";
    const char *PEOPLE_HUB_URL = "https://peoplehub.xboxlive.com";
    const char *SOCIAL_URL = "https://social.xboxlive.com";
    const char *PEOPLE_HUB_CONTRACT = "7";
    const char *SOCIAL_CONTRACT = "3";
    const char *PEOPLE_DECORATIONS = "/decoration/detail,presenceDetail";
    const char *SEARCH_DECORATIONS = "/decoration/detail,preferredColor";
    const int REQUEST_ATTEMPTS = 3;
    const int RETRY_MIN_DELAY_MS = 500;
    const int RETRY_MAX_DELAY_MS = 8000;
    const int WAIT_STEP_MS = 20;
    const size_t MAX_QUERY_LENGTH = 64;

    std::string text(const json::Value *object, const char *key) {
        const json::Value *value = object != nullptr ? object->get(key) : nullptr;
        if (value == nullptr)
            return {};

        if (value->isNumber())
            return std::to_string((long long) value->number());

        return value->string();
    }

    bool flag(const json::Value *object, const char *key) {
        const json::Value *value = object != nullptr ? object->get(key) : nullptr;
        return value != nullptr && value->boolean();
    }

    XboxPerson readPerson(const json::Value &person) {
        XboxPerson result;
        result.mXuid = text(&person, "xuid");
        result.mGamertag = text(&person, "uniqueModernGamertag");
        if (result.mGamertag.empty())
            result.mGamertag = text(&person, "gamertag");
        result.mDisplayName = text(&person, "displayName");
        if (result.mDisplayName.empty())
            result.mDisplayName = result.mGamertag;
        result.mDisplayPicture = text(&person, "displayPicRaw");
        result.mPresenceState = text(&person, "presenceState");
        result.mPresenceText = text(&person, "presenceText");
        result.mFriend = flag(&person, "isFriend");
        result.mFriendRequestReceived = flag(&person, "isFriendRequestReceived");
        result.mFriendRequestSent = flag(&person, "isFriendRequestSent");
        result.mFollowing = flag(&person, "isFollowedByCaller");
        result.mFollowedBy = flag(&person, "isFollowingCaller");
        result.mFavorite = flag(&person, "isFavorite");

        if (const json::Value *detail = person.get("detail"); detail != nullptr && detail->isObject()) {
            result.mCanBeFriended = flag(detail, "canBeFriended");
            result.mFriend = result.mFriend || flag(detail, "friend");
            result.mFriendRequestReceived = result.mFriendRequestReceived || flag(detail, "isFriendRequestReceived");
            result.mFriendRequestSent = result.mFriendRequestSent || flag(detail, "isFriendRequestSent");
        }

        if (const json::Value *titles = person.get("presenceDetails"); titles != nullptr && titles->isArray()) {
            for (const std::unique_ptr<json::Value> &title: titles->mArray) {
                if (title == nullptr || !title->isObject())
                    continue;

                XboxTitlePresence presence;
                presence.mTitleId = text(title.get(), "TitleId");
                presence.mState = text(title.get(), "State");
                presence.mDevice = text(title.get(), "Device");
                presence.mPresenceText = text(title.get(), "PresenceText");
                presence.mRichPresenceText = text(title.get(), "RichPresenceText");
                presence.mPrimary = flag(title.get(), "IsPrimary");
                presence.mGame = flag(title.get(), "IsGame");
                result.mTitles.push_back(std::move(presence));
            }
        }

        return result;
    }

    bool retryable(int status, bool mutation) {
        if (mutation)
            return status == 429;

        return status == 429 || status == 408 || (status >= 500 && status < 600);
    }

}

XboxSocialService::XboxSocialService(MinecraftAuthentication &authentication) : mAuthentication(authentication) {
}

bool XboxSocialService::_cancelled() const {
    return mCancel != nullptr && mCancel->load();
}

bool XboxSocialService::isValidXuid(const std::string &xuid) {
    return !xuid.empty() && xuid.size() <= 20 && std::all_of(xuid.begin(), xuid.end(), [](char c) {
        return std::isdigit((unsigned char) c) != 0;
    });
}

std::string XboxSocialService::sizedPicture(const std::string &url, int size) {
    if (url.empty())
        return {};

    std::string sized = url;
    if (sized.rfind("http://", 0) == 0)
        sized.replace(0, 7, "https://");

    const std::string plainHost = "https://images-eds.xboxlive.com";
    if (sized.rfind(plainHost, 0) == 0)
        sized.replace(0, plainHost.size(), "https://images-eds-ssl.xboxlive.com");

    if (sized.rfind("https://", 0) != 0)
        return {};

    const std::string dimensions = "w=" + std::to_string(size) + "&h=" + std::to_string(size);
    sized += sized.find('?') == std::string::npos ? "?" + dimensions : "&" + dimensions;
    return sized;
}

/**
 * Sends one request to an Xbox Live people service. Reads are retried after
 * network failures, rate limits and server errors with a growing delay that
 * honours Retry-After; a mutation only after a rate limit, since any other
 * failure leaves unknown whether it took effect.
 */
bool XboxSocialService::_request(const std::string &method, const std::string &url, const char *contract,
                                 bool mutation, int &outStatus, std::string &outBody, std::string &outError) {
    mLastError.clear();

    XboxLiveToken token;
    if (!mAuthentication.getXboxLiveAuthentication().requestToken(XBOX_LIVE_RELYING_PARTY, token, outError)) {
        mLastError.mKind = _cancelled() ? ServiceErrorKind::Cancelled : ServiceErrorKind::Unauthorized;
        outError = "request xbox live token: " + outError;
        return false;
    }

    HttpClient::Headers headers;
    headers.emplace_back("Authorization", token.getAuthorizationHeader());
    headers.emplace_back("x-xbl-contract-version", contract);
    headers.emplace_back("Accept", "application/json");
    headers.emplace_back("Accept-Language", mLanguage);
    if (mutation)
        headers.emplace_back("Cache-Control", "no-cache");

    int delay = RETRY_MIN_DELAY_MS;
    int retryAfter = 0;

    for (int attempt = 0; attempt < REQUEST_ATTEMPTS; ++attempt) {
        if (attempt > 0) {
            const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(std::max(delay, retryAfter));
            while (std::chrono::steady_clock::now() < until) {
                if (_cancelled()) {
                    mLastError.mKind = ServiceErrorKind::Cancelled;
                    outError = "request cancelled";
                    return false;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_STEP_MS));
            }
            delay = std::min(delay * 2, RETRY_MAX_DELAY_MS);
        }

        HttpResponse response;
        std::string error;
        if (!HttpClient::request(method, url, headers, std::string(), response, error,
                                 HttpClient::DEFAULT_TIMEOUT_MS, 16 * 1024 * 1024, mCancel)) {
            mLastError.mKind = ServiceError::classifyTransport(error, _cancelled());
            outError = error;
            if (mutation || mLastError.mKind == ServiceErrorKind::Cancelled)
                return false;
            continue;
        }

        outStatus = response.mStatus;
        outBody = std::move(response.mBody);
        const std::string header = response.getHeader("Retry-After");
        retryAfter = ServiceError::parseRetryAfter(header, RETRY_MAX_DELAY_MS);
        mLastError.clear();

        if (!retryable(outStatus, mutation) || attempt + 1 == REQUEST_ATTEMPTS) {
            if (outStatus >= 300) {
                mLastError.mKind = ServiceError::classify(outStatus);
                mLastError.mStatus = outStatus;
                mLastError.mRetryAfterMs = ServiceError::parseRetryAfter(header, 3600 * 1000);
                outError = "HTTP Error: " + std::to_string(outStatus);
                return false;
            }
            return true;
        }
    }

    return false;
}

bool XboxSocialService::_readPeople(const std::string &body, std::vector<XboxPerson> &outPeople,
                                    std::string &outError) {
    std::unique_ptr<json::Value> root = json::parse(body);
    const json::Value *people = root != nullptr ? root->get("people") : nullptr;

    if (root == nullptr || !root->isObject()) {
        mLastError.mKind = ServiceErrorKind::InvalidResponse;
        outError = "invalid people response";
        return false;
    }

    if (people == nullptr || !people->isArray())
        return true;

    for (const std::unique_ptr<json::Value> &person: people->mArray) {
        if (person == nullptr || !person->isObject())
            continue;

        XboxPerson parsed = readPerson(*person);
        if (isValidXuid(parsed.mXuid))
            outPeople.push_back(std::move(parsed));
    }

    return true;
}

bool XboxSocialService::requestPeople(XboxPeopleList list, std::vector<XboxPerson> &outPeople,
                                      std::string &outError) {
    outPeople.clear();

    const char *group = list == XboxPeopleList::Friends ? "friends"
                        : list == XboxPeopleList::ReceivedRequests ? "friendRequests(received)"
                                                                   : "friendRequests(sent)";
    const std::string url = std::string(PEOPLE_HUB_URL) + "/users/me/people/" + group + PEOPLE_DECORATIONS;

    int status = 0;
    std::string body;
    if (!_request("GET", url, PEOPLE_HUB_CONTRACT, false, status, body, outError))
        return false;

    return _readPeople(body, outPeople, outError);
}

bool XboxSocialService::search(const std::string &query, std::vector<XboxPerson> &outPeople,
                               std::string &outError) {
    outPeople.clear();
    mLastError.clear();

    if (query.empty() || query.size() > MAX_QUERY_LENGTH) {
        mLastError.mKind = ServiceErrorKind::Failed;
        outError = "invalid search";
        return false;
    }

    const std::string url = std::string(PEOPLE_HUB_URL) + "/users/me/people/search" + SEARCH_DECORATIONS + "?" +
                            HttpClient::encodeForm({{"q", query}});

    int status = 0;
    std::string body;
    if (!_request("GET", url, PEOPLE_HUB_CONTRACT, false, status, body, outError))
        return false;

    return _readPeople(body, outPeople, outError);
}

bool XboxSocialService::addFriend(const std::string &xuid, std::string &outError) {
    mLastError.clear();
    if (!isValidXuid(xuid)) {
        mLastError.mKind = ServiceErrorKind::NotFound;
        outError = "invalid xuid";
        return false;
    }

    const std::string url = std::string(SOCIAL_URL) + "/users/me/people/friends/v2/xuid(" + xuid + ")";
    int status = 0;
    std::string body;
    return _request("PUT", url, SOCIAL_CONTRACT, true, status, body, outError);
}

bool XboxSocialService::removeFriend(const std::string &xuid, std::string &outError) {
    mLastError.clear();
    if (!isValidXuid(xuid)) {
        mLastError.mKind = ServiceErrorKind::NotFound;
        outError = "invalid xuid";
        return false;
    }

    const std::string url =
            std::string(SOCIAL_URL) + "/users/me/people/friends/v2/xuid(" + xuid + ")?deleteRelationships=friends";
    int status = 0;
    std::string body;
    return _request("DELETE", url, SOCIAL_CONTRACT, true, status, body, outError);
}
