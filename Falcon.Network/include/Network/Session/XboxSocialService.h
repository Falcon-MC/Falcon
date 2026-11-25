#pragma once

#include "Network/Http/ServiceError.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

class MinecraftAuthentication;

struct XboxTitlePresence {
    std::string mTitleId;
    std::string mState;
    std::string mDevice;
    std::string mPresenceText;
    std::string mRichPresenceText;
    bool mPrimary = false;
    bool mGame = false;
};

/**
 * An Xbox Live user as the people service describes them to the signed in
 * player: who they are, their presence, and how the two are related.
 */
struct XboxPerson {
    std::string mXuid;
    std::string mGamertag;
    std::string mDisplayName;
    std::string mDisplayPicture;
    std::string mPresenceState;
    std::string mPresenceText;
    bool mFriend = false;
    bool mFriendRequestReceived = false;
    bool mFriendRequestSent = false;
    bool mFollowing = false;
    bool mFollowedBy = false;
    bool mFavorite = false;
    bool mCanBeFriended = false;
    std::vector<XboxTitlePresence> mTitles;
};

enum class XboxPeopleList {
    Friends,
    ReceivedRequests,
    SentRequests,
};

class XboxSocialService {
public:
    explicit XboxSocialService(MinecraftAuthentication &authentication);

    void setCancelFlag(const std::atomic<bool> *cancel) {
        mCancel = cancel;
    }

    /**
     * The language presence texts come back in, like "en-US".
     */
    void setLanguage(std::string language) {
        mLanguage = std::move(language);
    }

    const ServiceError &getLastError() const {
        return mLastError;
    }

    bool requestPeople(XboxPeopleList list, std::vector<XboxPerson> &outPeople, std::string &outError);

    bool search(const std::string &query, std::vector<XboxPerson> &outPeople, std::string &outError);

    /**
     * Sends a friend request, or accepts the one the user already sent.
     */
    bool addFriend(const std::string &xuid, std::string &outError);

    /**
     * Ends the friendship, or withdraws or declines the pending request
     * between the player and the user.
     */
    bool removeFriend(const std::string &xuid, std::string &outError);

    static bool isValidXuid(const std::string &xuid);

    static std::string sizedPicture(const std::string &url, int size);

private:
    bool _request(const std::string &method, const std::string &url, const char *contract, bool mutation,
                  int &outStatus, std::string &outBody, std::string &outError);

    bool _readPeople(const std::string &body, std::vector<XboxPerson> &outPeople, std::string &outError);

    bool _cancelled() const;

    MinecraftAuthentication &mAuthentication;
    const std::atomic<bool> *mCancel = nullptr;
    std::string mLanguage = "en-US";
    ServiceError mLastError;
};
