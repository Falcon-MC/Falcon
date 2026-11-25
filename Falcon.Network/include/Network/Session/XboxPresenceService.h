#pragma once

#include <string>

class MinecraftAuthentication;

/**
 * Keeps the signed in user shown as online in Minecraft, which friends need to
 * see before a session the user publishes appears in their friends list.
 */
class XboxPresenceService {
public:
    static const int DEFAULT_HEARTBEAT_SECONDS = 300;

    explicit XboxPresenceService(MinecraftAuthentication &authentication);

    /**
     * Marks the user as active in the title. On success, outHeartbeatSeconds
     * holds the delay Xbox Live asks for before the next update.
     */
    bool update(int &outHeartbeatSeconds, std::string &outError);

private:
    MinecraftAuthentication &mAuthentication;
};
