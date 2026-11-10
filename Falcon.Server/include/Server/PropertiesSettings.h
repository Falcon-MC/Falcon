#pragma once

#include "Network/NetworkEnums.h"
#include "Protocol/Packets/NetworkSettingsPacket.h"
#include "Protocol/Types/StartGameTypes.h"

#include <string>
#include <unordered_map>
#include <vector>

enum class Difficulty : int { Peaceful = 0, Easy = 1, Normal = 2, Hard = 3 };

enum class ContentLogLevel : int { Error = 0, Warning = 1, Info = 2, Verbose = 3 };

enum class PropertyKind { String, Bool, Int, Float, Enum };

struct PropertyDefinition {
    const char *mKey;
    const char *mDefault;
    PropertyKind mKind;
    int mMinimum;
    int mMaximum;
    const char *mValues;
};

// BDS-style server.properties; unknown keys kept: a rewrite doesn't lose settings this build ignores
class PropertiesSettings {
public:
    static const std::vector<PropertyDefinition> &getDefinitions();

    static const PropertyDefinition *findDefinition(const std::string &key);

    static std::string getDefaultContents();

    PropertiesSettings();

    explicit PropertiesSettings(const std::string &path);

    bool load(const std::string &path);

    bool isLoaded() const {
        return mLoaded;
    }

    const std::string &getPath() const {
        return mPath;
    }

    bool hasProperty(const std::string &key) const;

    std::string getString(const std::string &key, const std::string &defaultValue = std::string()) const;

    int getInt(const std::string &key, int defaultValue) const;

    float getFloat(const std::string &key, float defaultValue) const;

    bool getBool(const std::string &key, bool defaultValue) const;

    std::string getString(const char *key) const;

    int getInt(const char *key) const;

    float getFloat(const char *key) const;

    bool getBool(const char *key) const;

    const std::vector<std::string> &getInvalidProperties() const {
        return mInvalid;
    }

    std::string getServerName() const {
        return getString("server-name");
    }

    std::string getLanguage() const {
        return getString("language");
    }

    GameType getGameType() const;

    bool getForceGameType() const {
        return getBool("force-gamemode");
    }

    Difficulty getDifficulty() const;

    bool getAllowCheats() const {
        return getBool("allow-cheats");
    }

    int getMaxPlayers() const {
        return getInt("max-players");
    }

    bool getOnlineMode() const {
        return getBool("online-mode");
    }

    bool getNetworkEncryption() const {
        return getBool("network-encryption");
    }

    bool getAllowList() const {
        return getBool("allow-list");
    }

    unsigned short getServerPort() const {
        return (unsigned short) getInt("server-port");
    }

    unsigned short getServerPortV6() const {
        return (unsigned short) getInt("server-portv6");
    }

    std::string getSignalingCertificate() const {
        return getString("signaling-certificate");
    }

    std::string getSignalingPrivateKey() const {
        return getString("signaling-private-key");
    }

    unsigned short getSignalingExtraPort() const {
        return (unsigned short) getInt("signaling-extra-port");
    }

    std::string getSignalingAcmeDomain() const {
        return getString("signaling-acme-domain");
    }

    std::string getSignalingAcmeEmail() const {
        return getString("signaling-acme-email");
    }

    std::string getSignalingAcmeDirectory() const {
        return getString("signaling-acme-directory");
    }

    std::string getSignalingAcmeChallenge() const {
        return getString("signaling-acme-challenge");
    }

    bool getEnableLanVisibility() const {
        return getBool("enable-lan-visibility");
    }

    int getViewDistance() const {
        return getInt("view-distance");
    }

    int getTickDistance() const {
        return getInt("tick-distance");
    }

    int getPlayerIdleTimeout() const {
        return getInt("player-idle-timeout");
    }

    int getMaxThreads() const {
        return getInt("max-threads");
    }

    int getAutoSaveInterval() const {
        return getInt("autosave-interval");
    }

    std::string getLevelName() const {
        return getString("level-name");
    }

    std::string getLevelSeed() const {
        return getString("level-seed");
    }

    // comma-separated `worlds-autoload` list
    std::vector<std::string> getAutoloadWorlds() const;

    PlayerPermission getDefaultPlayerPermissionLevel() const;

    bool getTexturePackRequired() const {
        return getBool("texturepack-required");
    }

    // false: only the newest protocol may join
    bool getAnyVersion() const {
        return getBool("any-version");
    }

    bool getContentLogFileEnabled() const {
        return getBool("content-log-file-enabled");
    }

    bool getContentLogConsoleOutputEnabled() const {
        return getBool("content-log-console-output-enabled");
    }

    ContentLogLevel getContentLogLevel() const;

    unsigned short getCompressionThreshold() const {
        return (unsigned short) getInt("compression-threshold");
    }

    NetworkSettingsPacket::CompressionAlgorithm getCompressionAlgorithm() const;

    ChatRestrictionLevel getChatRestrictionLevel() const;

    bool getDisablePlayerInteraction() const {
        return getBool("disable-player-interaction");
    }

    bool getClientSideChunkGenerationEnabled() const {
        return getBool("client-side-chunk-generation-enabled");
    }

    // Comma-separated structure names, as StructureFeature registers them.
    std::string getClientSideChunkGenerationServerStructures() const {
        return getString("client-side-chunk-generation-server-structures");
    }

    /**
     * server-build-radius-ratio: the share of the view the server builds for client side generation clients, or a
     * negative value when it is "Disabled" and the ratio follows each client's hardware.
     */
    float getServerBuildRadiusRatio() const;

    bool getBlockNetworkIdsAreHashes() const {
        return getBool("block-network-ids-are-hashes");
    }

    bool getDisableCustomSkins() const {
        return getBool("disable-custom-skins");
    }

    bool getDisableClientVibrantVisuals() const {
        return getBool("disable-client-vibrant-visuals");
    }

    // seconds between two skin changes of one player
    int getSkinChangeCooldown() const {
        return getInt("skin-change-cooldown");
    }

    int getSpawnProtection() const {
        return getInt("spawn-protection");
    }

    int getAutoCompactionInterval() const {
        return getInt("auto-compaction-interval");
    }

    int getMaxInboundPacketsPerSecond() const {
        return getInt("max-inbound-packets-per-second");
    }

    int getMaxCommandsPerSecond() const {
        return getInt("max-commands-per-second");
    }

    int getMaxChatMessagesPerSecond() const {
        return getInt("max-chat-messages-per-second");
    }

    int getMaxFormResponsesPerSecond() const {
        return getInt("max-form-responses-per-second");
    }

    int getMaxMovementPacketsPerSecond() const {
        return getInt("max-movement-packets-per-second");
    }

    int getMaxChatMessageLength() const {
        return getInt("max-chat-message-length");
    }

    int getWasmPluginMemoryMb() const {
        return getInt("wasm-plugin-memory-mb");
    }

    int getWasmPluginFuel() const {
        return getInt("wasm-plugin-fuel");
    }

    float getPlayerPositionAcceptanceThreshold() const {
        return getFloat("player-position-acceptance-threshold");
    }

    // read on every player input: resolved once at load
    bool getMovementSimulation() const {
        return mMovementSimulation;
    }

    bool getServerAuthoritativeMovementStrict() const {
        return mServerAuthoritativeMovementStrict;
    }

    bool getServerAuthoritativeDismountStrict() const {
        return getBool("server-authoritative-dismount-strict");
    }

    float getPlayerMovementActionDirectionThreshold() const {
        return getFloat("player-movement-action-direction-threshold");
    }

    int getPlayerTickThrottledInputBatchSize() const {
        return getInt("player-tick-throttled-input-batch-size");
    }

    int getPlayerTickThrottledMaxTickCredits() const {
        return getInt("player-tick-throttled-max-tick-credits");
    }

    bool getServerAuthoritativeBlockBreaking() const {
        return getBool("server-authoritative-block-breaking");
    }

    float getServerAuthoritativeBlockBreakingRangeScalar() const;

    TransportLayer getTransportLayer() const;

    bool setProperty(const std::string &key, const std::string &value);

    // the way BDS reports them at startup
    std::string getUnknownContents() const;

private:
    static bool _isKnownProperty(const std::string &key);

    static std::string _trim(const std::string &value);

    static void _writeDefault(const std::string &path);

    static bool _isValidValue(const PropertyDefinition &definition, const std::string &value);

    void _validate();

    void _resolveMovementSettings();

    std::string mPath;
    bool mLoaded;
    bool mMovementSimulation = false;
    bool mServerAuthoritativeMovementStrict = false;
    std::unordered_map<std::string, std::string> mProperties;
    std::vector<std::string> mInvalid;
};
