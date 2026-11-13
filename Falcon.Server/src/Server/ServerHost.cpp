#include "Server/ServerHost.h"

#include "BuildInfo.h"
#include "World/Actor/AI/Goal/FollowCaravanGoal.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Spawn/SpawnRules.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Block/Inventory/EnderChestInventoryStore.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Debug/FileLogEndPoint.h"
#include "Core/Utility/UUID.h"
#include "World/Item/StringToItemParser.h"
#include "World/Level/Storage/AutoCompaction.h"
#include "World/Level/Storage/LevelStorage.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/Overworld/Biome/OverworldBiomeParameters.h"
#include "Network/ServerNetworkHandler.h"
#include "Network/TransportFactory.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Server/Localization.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <future>
#include <random>
#include <atomic>
#include <exception>
#include <stdexcept>
#include <thread>
#include "World/Level/WorldManager.h"

namespace {
    std::atomic<bool> processOwned{false};
    const char *const PROPERTIES_FILE = "server.properties";
    const char *const PROFILER_CONFIG_FILE = "bootstrap.json";
    const char *const CDN_CONFIG_FILE = "cdn_config.json";

    const char *const LOADING_WORLD_BANNER = "\n\n"
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

    // content has no log of its own: packs and scripts are told apart from the server by their log area
    class ContentLogFileEndPoint : public FileLogEndPoint {
    public:
        ContentLogFileEndPoint(const std::string &path, LogLevel minimumLevel)
                : FileLogEndPoint(path), mMinimumLevel(minimumLevel) {}

        void log(const LogDetails &details) override {
            if (details.mLevel < mMinimumLevel)
                return;
            if (details.mArea == LogAreaID::Script || details.mArea == LogAreaID::Resource)
                FileLogEndPoint::log(details);
        }

    private:
        LogLevel mMinimumLevel;
    };

    LogLevel toLogLevel(ContentLogLevel level) {
        switch (level) {
            case ContentLogLevel::Error:
                return LogLevel::Error;
            case ContentLogLevel::Warning:
                return LogLevel::Warning;
            case ContentLogLevel::Verbose:
                return LogLevel::Verbose;
            default:
                return LogLevel::Info;
        }
    }

    std::shared_ptr<ILogEndPoint> openContentLog(ContentLogLevel level) {
        const std::string directory = ServerPaths::file("logs");
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        if (error) {
            LOG_WARN(LogAreaID::Server,
                     "Content logging to disk is disabled because its directory could not be "
                     "created: %s",
                     directory.c_str());
            return nullptr;
        }

        char timestamp[32];
        const std::time_t now = std::time(nullptr);
        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", std::localtime(&now));

        const std::string path = directory + "/ContentLog_" + timestamp;
        auto endPoint = std::make_shared<ContentLogFileEndPoint>(path + ".txt", toLogLevel(level));
        if (!endPoint->isOpen()) {
            LOG_WARN(LogAreaID::Server, "Could not open the content log %s.txt", path.c_str());
            return nullptr;
        }

        LOG_INFO(LogAreaID::Server, "Content logging to disk is enabled. Writing log to: %s.txt", path.c_str());
        return endPoint;
    }

    // https on the port, plus 443 for 19132 only; http after every https probe ends
    const unsigned short DEFAULT_GAME_PORT = 19132;
    const unsigned short HTTPS_PORT = 443;

    // a half-set pair is ignored: only both paths take precedence over ACME
    bool hasCertificatePair(const PropertiesSettings &properties) {
        return !properties.getSignalingCertificate().empty() && !properties.getSignalingPrivateKey().empty();
    }

    // ACME validates on fixed ports whatever the game port: RFC 8737 on 443, RFC 8555 http-01 on 80
    void logAcmeNotice(const PropertiesSettings &properties, const ServerNetworkHandler &handler, unsigned short port) {
        const std::string domain = properties.getSignalingAcmeDomain();
        if (domain.empty() || hasCertificatePair(properties))
            return;

        const bool tlsAlpn = properties.getSignalingAcmeChallenge() != "http-01";
        const unsigned short needed = tlsAlpn ? 443 : 80;
        if (port != needed && handler.getExtraSignalingPort() != needed)
            LOG_INFO(LogAreaID::Server,
                     "ACME for %s validates on TCP %u: set signaling-extra-port=%u or forward "
                     "TCP %u to TCP %u of this server.",
                     domain.c_str(), (unsigned) needed, (unsigned) needed, (unsigned) needed, (unsigned) port);
        LOG_INFO(LogAreaID::Server, "%s must resolve to this server's public address.", domain.c_str());
    }

    void logTransportNotice(TransportLayer transport, const ServerNetworkHandler &handler, unsigned short port,
                            bool acmePending) {
        LOG_INFO(LogAreaID::Server, "==================== TRANSPORT =======================");
        LOG_INFO(LogAreaID::Server, "Connection type: %s", toString(transport));
        if (transport == TransportLayer::NetherNet) {
            if (handler.isSignalingSecure()) {
                LOG_INFO(LogAreaID::Server, "NetherNet serves its signaling endpoint over HTTPS and HTTP on TCP %u.",
                         (unsigned) port);
            } else {
                LOG_INFO(LogAreaID::Server, "NetherNet serves its signaling endpoint over plain HTTP on TCP %u.",
                         (unsigned) port);
                if (acmePending)
                    LOG_INFO(LogAreaID::Server, "HTTPS starts once the ACME certificate is issued.");
                // Falcon on 443 refuses TLS at once: no stalled probe left to warn about
                else if (port == DEFAULT_GAME_PORT && handler.getExtraSignalingPort() != HTTPS_PORT)
                    LOG_INFO(LogAreaID::Server, "Players first try HTTPS here and on TCP 443 of this host: a firewall "
                                                "that drops 443 delays every join by 3 s. Set signaling-extra-port=443,"
                                                " signaling-acme-domain, or signaling-certificate and "
                                                "signaling-private-key to answer over HTTPS.");
            }
            if (handler.getExtraSignalingPort() != 0)
                LOG_INFO(LogAreaID::Server, "NetherNet signaling also listens on TCP %u.",
                         (unsigned) handler.getExtraSignalingPort());
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

    void logAnyVersionIgnored() {
        LOG_ERROR(LogAreaID::Server, "================ ANY VERSION IGNORED ==================");
        LOG_ERROR(LogAreaID::Server, "any-version is on, but it is ignored: only the newest version can join.");
        LOG_ERROR(LogAreaID::Server, "The server removes clients that do not behave as the game does, and what the "
                                     "game does is only known for the newest version. An older client would be "
                                     "taken for a fake one.");
        LOG_ERROR(LogAreaID::Server, "To accept older versions, set allow-unofficial-clients=true: this turns the "
                                     "client checks off.");
        LOG_ERROR(LogAreaID::Server, "=======================================================");
    }

    void logAnyVersionWarning() {
        LOG_WARN(LogAreaID::Server, "================ ANY VERSION WARNING ==================");
        LOG_WARN(LogAreaID::Server, "any-version is enabled: clients of older protocol versions can join.");
        LOG_WARN(LogAreaID::Server, "Older versions still receive the blocks, items and recipes of the newest one, "
                                    "so they may see missing or wrong content. This is not recommended on a "
                                    "public server.");
        LOG_WARN(LogAreaID::Server, "=======================================================");
    }

    std::exception_ptr loadProperties(PropertiesSettings &properties, const std::string &path,
                                      const ServerHostOptions &options) {
        try {
            for (const auto &entry: options.properties)
                if (!properties.setProperty(entry.first, entry.second))
                    throw std::runtime_error("Could not write server property: " + entry.first);
            if (!properties.load(path))
                throw std::runtime_error("Could not read server.properties");
            if (!options.properties.empty() && !properties.getInvalidProperties().empty())
                throw std::invalid_argument("Invalid server property: " + properties.getInvalidProperties().front());
            if (!WorldManager::isValidName(properties.getLevelName()))
                throw std::invalid_argument("Invalid world name");
        } catch (...) {
            return std::current_exception();
        }
        return nullptr;
    }

    // start() adopts the database when it opens the world; on any earlier exit it must not keep the lock
    struct PreopenedLevelGuard {
        ~PreopenedLevelGuard() {
            LevelStorage::discardPreopened();
        }
    };

    // process-wide registries outlive a server; an embedder opens world after world:
    // none may see the previous one's players or packs
    void resetProcessState() {
        EnderChestInventoryStore::getInstance().clear();
        CustomContentRegistry::getInstance().reset();
        StringToItemParser::getInstance().reset();
        FollowCaravanGoal::clearCaravans();
    }
}

ServerHost::ServerHost() = default;

ServerHost::~ServerHost() {
    try {
        stop();
    } catch (...) {
    }
    release();
}

bool ServerHost::acquire() {
    if (mOwnsProcess)
        return true;
    bool expected = false;
    mOwnsProcess = processOwned.compare_exchange_strong(expected, true);
    return mOwnsProcess;
}

void ServerHost::release() noexcept {
    if (mOwnsProcess && mHandler == nullptr) {
        mOwnsProcess = false;
        processOwned.store(false);
    }
}

bool ServerHost::start(const ServerHostOptions &options) {
    if (mHandler != nullptr || !acquire())
        return false;

    // read first: the level database opens while the registries load; a bad file still throws after them
    const std::string propertiesPath = ServerPaths::file(PROPERTIES_FILE);
    PropertiesSettings properties(propertiesPath);
    const std::exception_ptr propertiesError = loadProperties(properties, propertiesPath, options);
    PreopenedLevelGuard preopenedLevel;
    if (propertiesError == nullptr)
        LevelStorage::preopen(WorldManager::getWorldsDirectory(), properties.getLevelName());

    // independent registries; joined before the handler exists: the game sees only complete definitions
    auto entityDefinitions = std::async(std::launch::async, EntityDefinitions::initialize);
    auto biomeData = std::async(std::launch::async, BiomeChunkGenDataRegistry::initialize);
    // function-local statics built ahead of their first use: a reader blocks until the build finishes
    auto languages = std::async(std::launch::async, Localization::getInstance);
    auto biomeParameters = std::async(std::launch::async, OverworldBiomeParameters::shared);
    BlockPaletteRegistry::getInstance().initialize();
    SpawnRules::initialize();
    entityDefinitions.get();
    biomeData.get();
    if (propertiesError != nullptr)
        std::rethrow_exception(propertiesError);

    languages.get();
    Localization::setServerLocale(properties.getLanguage());
    logStartupBanner(properties);

    if (properties.getContentLogFileEnabled()) {
        mContentLogEndPoint = openContentLog(properties.getContentLogLevel());
        if (mContentLogEndPoint != nullptr)
            BedrockLog::addEndPoint(mContentLogEndPoint);
    }

    const std::vector<std::string> &invalid = properties.getInvalidProperties();
    if (std::find(invalid.begin(), invalid.end(), "transport=raknet") != invalid.end())
        LOG_ERROR(LogAreaID::Server,
                  "RakNet is no longer supported, set transport=nethernet in %s. "
                  "Falling back to NetherNet.",
                  PROPERTIES_FILE);

    if (properties.getServerAuthoritativeMovementStrict() && !properties.getMovementSimulation())
        LOG_WARN(LogAreaID::Server,
                 "server-authoritative-movement-strict has no effect while movement-simulation "
                 "is false in %s",
                 PROPERTIES_FILE);

    const TransportLayer transport = properties.getTransportLayer();

    if (transport == TransportLayer::Unknown) {
        LOG_FATAL(LogAreaID::Server, "Unknown 'transport' value in %s, expected 'nethernet'", PROPERTIES_FILE);
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

    biomeParameters.get();
    mHandler.reset(new ServerNetworkHandler(serverName, options.subMotd, maxPlayers, transport));
    mHandler->setProtocolVersion(options.protocolVersion, options.gameVersion);
    mHandler->setPluginsEnabled(options.plugins);
    mHandler->setProperties(properties);

    if (properties.getAllowList() && mHandler->getAllowList().isEmpty())
        logAllowListWarning();

    if (properties.isAnyVersionRequested() && !properties.getAllowUnofficialClients())
        logAnyVersionIgnored();
    else if (properties.getAnyVersion())
        logAnyVersionWarning();

    unsigned short port = properties.isLoaded() ? properties.getServerPort() : options.port;
    const unsigned short portV6 = properties.isLoaded() ? properties.getServerPortV6() : options.portV6;
    if (options.portOverride)
        port = *options.portOverride;

    ConnectionDefinition definition = ConnectionDefinition::createFromPorts(port, portV6, maxPlayers);
    if (!options.bindAddress.empty()) {
        definition.mIPv4Address = options.bindAddress;
        definition.mNeedsHostDiscovery = false;
    }

    if (!mHandler->startServerListening(definition)) {
        LOG_FATAL(LogAreaID::Server, "Failed to start server");
        BedrockLog::flush();
        mHandler.reset();
        resetProcessState();
        return false;
    }

    mPort = mHandler->getBoundPort() != 0 ? mHandler->getBoundPort() : port;

    LOG_INFO(LogAreaID::Server, "Server started.");
    logTransportNotice(transport, *mHandler, mPort,
                       !properties.getSignalingAcmeDomain().empty() && !hasCertificatePair(properties));
    logAcmeNotice(properties, *mHandler, mPort);
    BedrockLog::flush();

    AutoCompaction::start(properties.getAutoCompactionInterval());
    return true;
}

void ServerHost::tick(bool paused) {
    if (mHandler != nullptr && !mShutDown)
        mHandler->tick(paused);
}

void ServerHost::waitForNextTick(std::chrono::steady_clock::time_point nextTick) {
    const std::chrono::nanoseconds pollInterval(2000000);

    for (;;) {
        if (mHandler != nullptr && !mShutDown)
            mHandler->pumpLoginEvents();

        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if (now >= nextTick)
            return;

        std::this_thread::sleep_for(std::min<std::chrono::nanoseconds>(nextTick - now, pollInterval));
    }
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

void ServerHost::_closeContentLog() {
    if (mContentLogEndPoint == nullptr)
        return;

    BedrockLog::flush();
    BedrockLog::removeEndPoint(mContentLogEndPoint);
    mContentLogEndPoint.reset();
}

void ServerHost::stop() {
    if (mHandler == nullptr) {
        _closeContentLog();
        return;
    }

    std::exception_ptr error;
    try {
        shutdown();
    } catch (...) {
        error = std::current_exception();
    }
    for (World *world: mHandler->getWorlds().getWorlds())
        AutoCompaction::untrack(world->getOverworld());
    mHandler.reset();
    mShutDown = false;
    mPort = 0;
    resetProcessState();
    BedrockLog::flush();
    _closeContentLog();
    if (error)
        std::rethrow_exception(error);
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
