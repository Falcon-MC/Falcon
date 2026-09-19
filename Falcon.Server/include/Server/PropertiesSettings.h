#pragma once

#include "Network/NetworkEnums.h"
#include "Protocol/Packets/NetworkSettingsPacket.h"
#include "Protocol/Types/StartGameTypes.h"

#include <string>
#include <unordered_map>

enum class Difficulty : int {
    Peaceful = 0,
    Easy = 1,
    Normal = 2,
    Hard = 3
};

enum class ContentLogLevel : int {
    Error = 0,
    Warning = 1,
    Info = 2,
    Verbose = 3
};

// Reads the BDS style server.properties file. Unknown keys are kept so that a rewrite does not
// lose settings this build does not use yet.
class PropertiesSettings {
public:
    PropertiesSettings();

    explicit PropertiesSettings(const std::string &path);

    bool load(const std::string &path);

    bool isLoaded() const { return mLoaded; }

    const std::string &getPath() const { return mPath; }

    // raw access
    bool hasProperty(const std::string &key) const;

    std::string getString(const std::string &key, const std::string &defaultValue = std::string()) const;

    int getInt(const std::string &key, int defaultValue) const;

    float getFloat(const std::string &key, float defaultValue) const;

    bool getBool(const std::string &key, bool defaultValue) const;

    // typed access, named after the properties they map to
    std::string getServerName() const { return getString("server-name", "Dedicated Server"); }

    GameType getGameType() const;

    bool getForceGameType() const { return getBool("force-gamemode", false); }

    Difficulty getDifficulty() const;

    bool getAllowCheats() const { return getBool("allow-cheats", true); }

    int getMaxPlayers() const { return getInt("max-players", 10); }

    bool getOnlineMode() const { return getBool("online-mode", true); }

    bool getAllowList() const { return getBool("allow-list", false); }

    unsigned short getServerPort() const { return (unsigned short) getInt("server-port", 19132); }

    unsigned short getServerPortV6() const { return (unsigned short) getInt("server-portv6", 19133); }

    bool getEnableLanVisibility() const { return getBool("enable-lan-visibility", true); }

    int getViewDistance() const { return getInt("view-distance", 32); }

    int getTickDistance() const { return getInt("tick-distance", 4); }

    int getPlayerIdleTimeout() const { return getInt("player-idle-timeout", 30); }

    int getMaxThreads() const { return getInt("max-threads", 8); }

    std::string getLevelName() const { return getString("level-name", "Bedrock level"); }

    std::string getLevelSeed() const { return getString("level-seed"); }

    PlayerPermission getDefaultPlayerPermissionLevel() const;

    bool getTexturePackRequired() const { return getBool("texturepack-required", false); }

    bool getContentLogFileEnabled() const { return getBool("content-log-file-enabled", false); }

    bool getContentLogConsoleOutputEnabled() const { return getBool("content-log-console-output-enabled", false); }

    ContentLogLevel getContentLogLevel() const;

    unsigned short getCompressionThreshold() const { return (unsigned short) getInt("compression-threshold", 1); }

    NetworkSettingsPacket::CompressionAlgorithm getCompressionAlgorithm() const;

    ChatRestrictionLevel getChatRestrictionLevel() const;

    bool getDisablePlayerInteraction() const { return getBool("disable-player-interaction", false); }

    bool getClientSideChunkGenerationEnabled() const { return getBool("client-side-chunk-generation-enabled", false); }

    bool getBlockNetworkIdsAreHashes() const { return getBool("block-network-ids-are-hashes", true); }

    bool getDisableCustomSkins() const { return getBool("disable-custom-skins", false); }

    float getPlayerPositionAcceptanceThreshold() const {
        return getFloat("player-position-acceptance-threshold", 0.5f);
    }

    float getPlayerPositionAcceptanceThresholdScaled() const {
        return getPlayerPositionAcceptanceThreshold() / 100.0f;
    }

    float getPlayerPositionAcceptanceThresholdSquared() const {
        const float scaled = getPlayerPositionAcceptanceThresholdScaled();
        return scaled * scaled;
    }

    float getPlayerMovementActionDirectionThreshold() const {
        return getFloat("player-movement-action-direction-threshold", 0.85f);
    }

    TransportLayer getTransportLayer() const;

    bool setProperty(const std::string &key, const std::string &value);

    // Renders the properties this build does not know about, the way BDS reports them at startup.
    std::string getUnknownContents() const;

private:
    static bool _isKnownProperty(const std::string &key);

    static std::string _trim(const std::string &value);

    static void _writeDefault(const std::string &path);

    std::string mPath;
    bool mLoaded;
    std::unordered_map<std::string, std::string> mProperties;
};
