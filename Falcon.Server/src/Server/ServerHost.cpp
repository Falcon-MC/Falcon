#include "Server/ServerHost.h"

#include "BuildInfo.h"
#include "Actor/AI/Goal/FollowCaravanGoal.h"
#include "Actor/Definition/EntityDefinitions.h"
#include "Actor/Spawn/SpawnRules.h"
#include "Block/BlockPaletteRegistry.h"
#include "Block/Inventory/EnderChestInventoryStore.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Utility/UUID.h"
#include "Item/StringToItemParser.h"
#include "Level/AutoCompaction.h"
#include "Level/Generator/Biome/BiomeChunkGenDataRegistry.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Network/TransportFactory.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Server/Localization.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"

#include <fstream>
#include <random>

namespace {
    const char *const PROPERTIES_FILE = "server.properties";
    const char *const PROFILER_CONFIG_FILE = "bootstrap.json";
    const char *const CDN_CONFIG_FILE = "cdn_config.json";

    const char *const LOADING_WORLD_BANNER =
            "\n\n"
            "#####################################################\n"
            "#                                                   #\n"
            "#               LOADING VANILLA WORLD               #\n"
            "#                                                   #\n"
            "#####################################################";

    std::string generateSessionId() {
        std::random_device device;
        std::mt19937_64 generator(((uint64_t) device() << 32) ^ device());
        std::uniform_int_distribution<uint64_t> distribution;

        uint64_t most = distribution(generator);
        uint64_t least = distribution(generator);

        most = (most & 0xffffffffffff0fffULL) | 0x0000000000004000ULL;
        least = (least & 0x3fffffffffffffffULL) | 0x8000000000000000ULL;

        return Uuid(most, least).toString();
    }

    bool fileExists(const std::string &path) {
        std::ifstream file(path);
        return file.is_open();
    }

    const char *toString(GameType gameType) {
        switch (gameType) {
            case GameType::Creative:
                return "Creative";
            case GameType::Adventure:
                return "Adventure";
            case GameType::Spectator:
                return "Spectator";
            default:
                return "Survival";
        }
    }

    const char *toString(Difficulty difficulty) {
        switch (difficulty) {
            case Difficulty::Peaceful:
                return "PEACEFUL";
            case Difficulty::Normal:
                return "NORMAL";
            case Difficulty::Hard:
                return "HARD";
            default:
                return "EASY";
        }
    }

    void logStartupBanner(const PropertiesSettings &properties) {
        LOG_INFO(LogAreaID::Server, "Starting Server");
        LOG_INFO(LogAreaID::Server, "Version: %s", FalconBuildInfo::kVersion);
        LOG_INFO(LogAreaID::Server, "Session ID: %s", generateSessionId().c_str());
        LOG_INFO(LogAreaID::Server, "Build ID: %s", FalconBuildInfo::kBuildId);
        LOG_INFO(LogAreaID::Server, "Branch: %s", FalconBuildInfo::kBranch);
        LOG_INFO(LogAreaID::Server, "Commit ID: %s", FalconBuildInfo::kCommitId);
        LOG_INFO(LogAreaID::Server, "Configuration: %s", FalconBuildInfo::kConfiguration);
        LOG_INFO(LogAreaID::Server, "Contents of %s: %s", PROPERTIES_FILE, properties.getUnknownContents().c_str());

        for (const std::string &invalid: properties.getInvalidProperties())
            LOG_WARN(LogAreaID::Server, "Invalid value in %s (%s), using the default", PROPERTIES_FILE,
                     invalid.c_str());
        LOG_INFO(LogAreaID::Server, "Level Name: %s", properties.getLevelName().c_str());
        LOG_INFO(LogAreaID::Server, "Profiler config ('%s') load result: success=%d, errorMessage=(null)",
                 PROFILER_CONFIG_FILE, fileExists(ServerPaths::file(PROFILER_CONFIG_FILE)) ? 1 : 0);

        if (!fileExists(ServerPaths::file(CDN_CONFIG_FILE)))
            LOG_INFO(LogAreaID::Server, "No CDN config file found at: %s for dedicated server", CDN_CONFIG_FILE);

        LOG_INFO(LogAreaID::Server, "Game mode: %d %s", (int) properties.getGameType(),
                 toString(properties.getGameType()));
        LOG_INFO(LogAreaID::Server, "Difficulty: %d %s", (int) properties.getDifficulty(),
                 toString(properties.getDifficulty()));

        if (!properties.getContentLogConsoleOutputEnabled())
            LOG_WARN(LogAreaID::Server, "Content logging to console is disabled.  Enable it with "
                                        "content-log-console-output-enabled=true in server.properties");

        LOG_INFO(LogAreaID::Server, "%s", LOADING_WORLD_BANNER);
    }

    void logTransportNotice(TransportLayer transport) {
        LOG_INFO(LogAreaID::Server, "==================== TRANSPORT =======================");
        LOG_INFO(LogAreaID::Server, "Connection type: %s", toString(transport));
        if (transport == TransportLayer::NetherNet) {
            LOG_INFO(LogAreaID::Server, "NetherNet serves its signaling endpoint over plain HTTP.");
        } else {
            LOG_INFO(LogAreaID::Server, "RakNet transport is active.");
        }
        LOG_INFO(LogAreaID::Server, "======================================================");
    }

    void logAllowListWarning() {
        LOG_WARN(LogAreaID::Server, "================ ALLOW LIST WARNING ===================");
        LOG_WARN(LogAreaID::Server, "Allow list is enabled but contains no entries. ");
        LOG_WARN(LogAreaID::Server, "Use allowlist add <playername> for you and your friends so that they can access "
                                    "the server, or modify allowlist.json manually.");
        LOG_WARN(LogAreaID::Server, "\nAlternatively, the allow list can be turned off by typing allowlist off or "
                                    "manually toggled in the server.properties file.");
        LOG_WARN(LogAreaID::Server, "=======================================================");
    }

    void logAnyVersionWarning() {
        LOG_WARN(LogAreaID::Server, "================ ANY VERSION WARNING ==================");
        LOG_WARN(LogAreaID::Server, "any-version is enabled: clients of older protocol versions can join.");
        LOG_WARN(LogAreaID::Server, "Older versions still receive the blocks, items and recipes of the newest one, "
                                    "so they may see missing or wrong content. This is not recommended on a "
                                    "public server.");
        LOG_WARN(LogAreaID::Server, "=======================================================");
    }

    /**
     * State that outlives a server in process wide registries. The dedicated server never restarts, but an
     * embedding host opens one world after another and must not see the previous one's players or packs.
     */
    void resetProcessState() {
        EnderChestInventoryStore::getInstance().clear();
        CustomContentRegistry::getInstance().reset();
        StringToItemParser::getInstance().reset();
        FollowCaravanGoal::clearCaravans();
    }
}

ServerHost::ServerHost() = default;

ServerHost::~ServerHost() {
    stop();
}

bool ServerHost::start(const ServerHostOptions &options) {
    if (mHandler != nullptr)
        return false;

    BiomeChunkGenDataRegistry::initialize();
    BlockPaletteRegistry::getInstance().initialize();
    SpawnRules::initialize();
    EntityDefinitions::initialize();

    const std::string propertiesPath = ServerPaths::file(PROPERTIES_FILE);
    PropertiesSettings properties(propertiesPath);
    for (const auto &entry: options.properties)
        properties.setProperty(entry.first, entry.second);

    Localization::setServerLocale(properties.getLanguage());
    logStartupBanner(properties);

    const TransportLayer transport = properties.getTransportLayer();

    if (transport == TransportLayer::Unknown) {
        LOG_FATAL(LogAreaID::Server, "Unknown 'transport' value in %s, expected 'raknet' or 'nethernet'",
                  PROPERTIES_FILE);
        BedrockLog::flush();
        return false;
    }

    if (!TransportFactory::isSupported(transport)) {
        LOG_FATAL(LogAreaID::Server, "Transport %s is not supported by this build", toString(transport));
        BedrockLog::flush();
        return false;
    }

    const int maxPlayers = properties.isLoaded() ? properties.getMaxPlayers() : options.maxPlayers;
    const std::string serverName = properties.isLoaded() ? properties.getServerName() : options.serverName;

    mHandler.reset(new ServerNetworkHandler(serverName, options.subMotd, maxPlayers, transport));
    mHandler->setProtocolVersion(options.protocolVersion, options.gameVersion);
    mHandler->setPluginsEnabled(options.plugins);
    mHandler->setLocalOnly(options.localOnly);
    mHandler->setProperties(properties);

    if (properties.getAllowList() && mHandler->getAllowList().isEmpty())
        logAllowListWarning();

    if (properties.getAnyVersion())
        logAnyVersionWarning();

    unsigned short port = properties.isLoaded() ? properties.getServerPort() : options.port;
    const unsigned short portV6 = properties.isLoaded() ? properties.getServerPortV6() : options.portV6;
    if (options.portOverride != 0)
        port = options.portOverride;

    const ConnectionDefinition definition = ConnectionDefinition::createFromPorts(port, portV6, maxPlayers);

    if (!mHandler->startServerListening(definition)) {
        LOG_FATAL(LogAreaID::Server, "Failed to start server");
        BedrockLog::flush();
        mHandler.reset();
        resetProcessState();
        return false;
    }

    mPort = port;

    LOG_INFO(LogAreaID::Server, "Server started.");
    logTransportNotice(transport);
    BedrockLog::flush();

    AutoCompaction::start(properties.getAutoCompactionInterval());
    return true;
}

void ServerHost::tick() {
    if (mHandler != nullptr && !mShutDown)
        mHandler->tick();
}

void ServerHost::shutdown() {
    if (mHandler == nullptr || mShutDown)
        return;

    LOG_INFO(LogAreaID::Server, "Shutting down...");
    AutoCompaction::stop();
    mHandler->stopServerListening();
    mHandler->getNetworkHandler().disconnect();
    mShutDown = true;
    BedrockLog::flush();
}

void ServerHost::stop() {
    if (mHandler == nullptr)
        return;

    shutdown();
    mHandler.reset();
    mShutDown = false;
    mPort = 0;
    resetProcessState();
    BedrockLog::flush();
}

bool ServerHost::isRunning() const {
    return mHandler != nullptr && !mShutDown;
}

bool ServerHost::isStopRequested() const {
    return mHandler != nullptr && mHandler->isStopRequested();
}

ServerNetworkHandler &ServerHost::getHandler() {
    return *mHandler;
}

unsigned short ServerHost::getPort() const {
    return mPort;
}
