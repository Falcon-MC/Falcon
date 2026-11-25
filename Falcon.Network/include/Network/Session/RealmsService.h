#pragma once

#include "Network/Http/ServiceError.h"
#include "Network/Session/SessionConnectionTarget.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

class MinecraftAuthentication;

struct RealmPlayer {
    std::string mUuid;
    std::string mName;
    std::string mPermission;
    bool mOperator = false;
    bool mAccepted = false;
    bool mOnline = false;
};

struct RealmDescription {
    long long mId = 0;
    std::string mName;
    std::string mOwner;
    std::string mOwnerUuid;
    std::string mMotd;
    std::string mState;
    std::string mDefaultPermission;
    std::string mWorldType;
    std::string mRemoteSubscriptionId;
    bool mExpired = false;
    bool mExpiredTrial = false;
    bool mGracePeriod = false;
    bool mMember = false;
    int mDaysLeft = 0;
    int mMaxPlayers = 0;
    int64_t mClubId = 0;
    std::vector<RealmPlayer> mPlayers;
};

struct RealmAddress {
    std::string mAddress;
    std::string mNetworkProtocol;
    bool mPendingUpdate = false;

    bool toTarget(SessionConnectionTarget &outTarget, std::string &outError) const;
};

/**
 * A membership invitation to someone else's Realm, waiting for the player to
 * accept or decline it.
 */
struct RealmInvite {
    std::string mInvitationId;
    std::string mWorldName;
    std::string mWorldDescription;
    std::string mOwnerName;
    std::string mOwnerUuid;
    int64_t mDate = 0;
};

class RealmsService {
public:
    explicit RealmsService(MinecraftAuthentication &authentication);

    /**
     * Makes every request give up as soon as the flag is set, instead of
     * waiting for the HTTP call or the retry delay to finish.
     */
    void setCancelFlag(const std::atomic<bool> *cancel) {
        mCancel = cancel;
    }

    /**
     * What went wrong with the last call that failed, cleared by every call.
     */
    const ServiceError &getLastError() const {
        return mLastError;
    }

    bool requestRealms(std::vector<RealmDescription> &outRealms, std::string &outError);

    bool requestRealmByCode(const std::string &code, RealmDescription &outRealm, std::string &outError);

    bool acceptInviteCode(const std::string &code, RealmDescription &outRealm, std::string &outError);

    bool requestPendingInvites(std::vector<RealmInvite> &outInvites, std::string &outError);

    bool acceptInvite(const std::string &invitationId, std::string &outError);

    bool rejectInvite(const std::string &invitationId, std::string &outError);

    bool requestOnlinePlayers(long long realmId, std::vector<RealmPlayer> &outPlayers, std::string &outError);

    bool requestAddress(long long realmId, unsigned int timeoutMs, const std::atomic<bool> *cancel,
                        RealmAddress &outAddress, std::string &outError);

    /**
     * The invite code of a realm link: "realm/CODE", or a realms.gg address
     * with or without its scheme, whose host is realms.gg itself and whose
     * path is the code alone. Empty when the text is not such a link.
     */
    static std::string inviteCode(const std::string &text);

    /**
     * The invite code of a realm link, or of the code typed on its own when
     * allowBareCode is set; surrounding spaces are ignored. Empty when the
     * text is neither.
     */
    static std::string parseInvite(const std::string &text, bool allowBareCode);

    static bool isValidInviteCode(const std::string &code);

private:
    bool _request(const std::string &method, const std::string &path, bool mutation, int &outStatus,
                  std::string &outBody, std::string &outError);

    bool _fail(int status, const std::string &body, std::string &outError);

    bool _cancelled() const;

    bool _wait(int milliseconds) const;

    MinecraftAuthentication &mAuthentication;
    const std::atomic<bool> *mCancel = nullptr;
    ServiceError mLastError;
};
