#pragma once

#include <cstdint>
#include <string>

/**
 * What a Bedrock server advertises to the server list, read from its answer to
 * an unconnected RakNet ping.
 */
struct ServerQueryResult {
    std::string mMotd;
    std::string mSubMotd;
    std::string mVersion;
    int mProtocol = 0;
    int mPlayerCount = 0;
    int mMaxPlayers = 0;
    int64_t mLatencyMs = 0;
};

class ServerQuery {
public:
    static const unsigned int DEFAULT_TIMEOUT_MS = 3000;

    static bool query(const std::string &host, unsigned short port, ServerQueryResult &outResult,
                      std::string &outError, unsigned int timeoutMs = DEFAULT_TIMEOUT_MS);

    /**
     * Fills a result from the semicolon separated announcement a server sends
     * back, "MCPE;motd;protocol;version;players;max;guid;sub motd;...".
     */
    static void readAnnouncement(const std::string &announcement, ServerQueryResult &outResult);
};
