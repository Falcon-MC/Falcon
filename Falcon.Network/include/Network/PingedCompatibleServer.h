#pragma once

#include <string>

struct PingedCompatibleServer {
    std::string mServerName;
    std::string mSubName;
    std::string mAddress;
    std::string mGameVersion;
    std::string mGameMode;
    unsigned short mPort;
    unsigned int mPing;
    int mProtocolVersion;
    int mServerAvailability;
    int mCurrentPlayers;
    int mMaxPlayers;
    unsigned long long mServerId;

    PingedCompatibleServer()
            : mPort(0), mPing(0), mProtocolVersion(0), mServerAvailability(1), mCurrentPlayers(0), mMaxPlayers(0),
              mServerId(0) {}

    std::string toAnnouncement(unsigned short portV4, unsigned short portV6) const;

    static bool parse(const std::string &rawAnnouncement, PingedCompatibleServer &outServer);
};
