#include "Server.h"

#include "BuildInfo.h"
#include "Server/VersionChecker.h"
#include "Core/Utility/UUID.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Debug/ContentLogEndPoint.h"
#include "Core/Debug/FileLogEndPoint.h"
#include "Actor/Spawn/SpawnRules.h"
#include "Block/BlockPaletteRegistry.h"
#include "Level/AutoCompaction.h"
#include "Level/Generator/Biome/BiomeChunkGenDataRegistry.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Network/TransportFactory.h"
#include "Server/Localization.h"
#include "Server/PropertiesSettings.h"
#include "Server/SetupWizard.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <random>
#include <thread>

#ifndef _WIN32
#include <poll.h>
#include <unistd.h>
#endif

static const char *PROPERTIES_FILE = "server.properties";
static const char *OPS_FILE = "ops.txt";
static const char *ALLOWLIST_FILE = "allowlist.json";
static const char *PROFILER_CONFIG_FILE = "bootstrap.json";
static const char *CDN_CONFIG_FILE = "cdn_config.json";

static const char *LOADING_WORLD_BANNER =
        "\n\n"
        "#####################################################\n"
        "#                                                   #\n"
        "#               LOADING VANILLA WORLD               #\n"
        "#                                                   #\n"
        "#####################################################";

static std::atomic<bool> gRunning(true);

static void requestShutdown(int signalNumber) {
    (void) signalNumber;
    gRunning.store(false);
}

static const int CONSOLE_POLL_INTERVAL = 200;

// Blocking on std::cin keeps the stdin lock held, which deadlocks the stream flush that runs while
// the process exits. Waiting on the descriptor first leaves the lock free until a line is ready.
static bool waitForConsoleInput(int timeoutMilliseconds) {
#ifdef _WIN32
    (void) timeoutMilliseconds;
    return true;
#else
    struct pollfd input;
    input.fd = STDIN_FILENO;
    input.events = POLLIN;
    input.revents = 0;

    return poll(&input, 1, timeoutMilliseconds) > 0;
#endif
}

static bool fileExists(const char *path);

static void setupServerLogging() {
    static const char *LOG_FILE = "server.log";

    if (!fileExists(LOG_FILE)) {
        printf("NO LOG FILE! - setting up server logging...\n");
        fflush(stdout);
    }

    BedrockLog::setLogLevel(LogLevel::Info);
    BedrockLog::addEndPoint(std::make_shared<ContentLogEndPoint>());

    std::shared_ptr<FileLogEndPoint> fileEndPoint = std::make_shared<FileLogEndPoint>(LOG_FILE);
    if (fileEndPoint->isOpen())
        BedrockLog::addEndPoint(fileEndPoint);
}

static std::string generateSessionId() {
    std::random_device device;
    std::mt19937_64 generator(((uint64_t) device() << 32) ^ device());
    std::uniform_int_distribution<uint64_t> distribution;

    uint64_t most = distribution(generator);
    uint64_t least = distribution(generator);

    most = (most & 0xffffffffffff0fffULL) | 0x0000000000004000ULL;
    least = (least & 0x3fffffffffffffffULL) | 0x8000000000000000ULL;

    return Uuid(most, least).toString();
}

static bool fileExists(const char *path) {
    std::ifstream file(path);
    return file.is_open();
}

static const char *toString(GameType gameType) {
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

static const char *toString(Difficulty difficulty) {
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

static void logStartupBanner(const PropertiesSettings &properties) {
    LOG_INFO(LogAreaID::Server, "Starting Server");
    LOG_INFO(LogAreaID::Server, "Version: %s", FalconBuildInfo::kVersion);
    LOG_INFO(LogAreaID::Server, "Session ID: %s", generateSessionId().c_str());
    LOG_INFO(LogAreaID::Server, "Build ID: %s", FalconBuildInfo::kBuildId);
    LOG_INFO(LogAreaID::Server, "Branch: %s", FalconBuildInfo::kBranch);
    LOG_INFO(LogAreaID::Server, "Commit ID: %s", FalconBuildInfo::kCommitId);
    LOG_INFO(LogAreaID::Server, "Configuration: %s", FalconBuildInfo::kConfiguration);
    LOG_INFO(LogAreaID::Server, "Contents of %s: %s", PROPERTIES_FILE, properties.getUnknownContents().c_str());

    for (const std::string &invalid: properties.getInvalidProperties())
        LOG_WARN(LogAreaID::Server, "Invalid value in %s (%s), using the default", PROPERTIES_FILE, invalid.c_str());
    LOG_INFO(LogAreaID::Server, "Level Name: %s", properties.getLevelName().c_str());
    LOG_INFO(LogAreaID::Server, "Profiler config ('%s') load result: success=%d, errorMessage=(null)",
             PROFILER_CONFIG_FILE, fileExists(PROFILER_CONFIG_FILE) ? 1 : 0);

    if (!fileExists(CDN_CONFIG_FILE))
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

static void logTransportNotice(TransportLayer transport) {
    LOG_INFO(LogAreaID::Server, "==================== TRANSPORT =======================");
    LOG_INFO(LogAreaID::Server, "Connection type: %s", toString(transport));
    if (transport == TransportLayer::NetherNet) {
        LOG_INFO(LogAreaID::Server, "NetherNet serves its signaling endpoint over plain HTTP.");
    } else {
        LOG_INFO(LogAreaID::Server, "RakNet transport is active.");
    }
    LOG_INFO(LogAreaID::Server, "======================================================");
}

static void logTelemetryNotice() {
    LOG_INFO(LogAreaID::Server, "================ TELEMETRY MESSAGE ===================");
    LOG_INFO(LogAreaID::Server, "Server Telemetry is currently not enabled. ");
    LOG_INFO(LogAreaID::Server, "Enabling this telemetry helps us improve the game.");
    LOG_INFO(LogAreaID::Server, "%s", "");
    LOG_INFO(LogAreaID::Server, "To enable this feature, add the line 'emit-server-telemetry=true'");
    LOG_INFO(LogAreaID::Server, "to the server.properties file in the handheld/src-server directory");
    LOG_INFO(LogAreaID::Server, "======================================================");
}

void startServer(const ServerSettings &settings) {
    setupServerLogging();

    BiomeChunkGenDataRegistry::initialize();
    BlockPaletteRegistry::getInstance().initialize();
    SpawnRules::initialize();

    if (settings.runSetupWizard && SetupWizard::isInteractive() && SetupWizard::isNeeded(PROPERTIES_FILE)) {
        SetupWizard wizard(PROPERTIES_FILE, OPS_FILE, ALLOWLIST_FILE);
        if (!wizard.run(settings.acceptLicense, settings.language)) {
            BedrockLog::shutdown();
            return;
        }
    }

    PropertiesSettings properties(PROPERTIES_FILE);
    Localization::setServerLocale(properties.getLanguage());
    logStartupBanner(properties);

    const TransportLayer transport = properties.getTransportLayer();

    if (transport == TransportLayer::Unknown) {
        LOG_FATAL(LogAreaID::Server, "Unknown 'transport' value in %s, expected 'raknet' or 'nethernet'",
                  PROPERTIES_FILE);
        BedrockLog::flush();
        return;
    }

    if (!TransportFactory::isSupported(transport)) {
        LOG_FATAL(LogAreaID::Server, "Transport %s is not supported by this build", toString(transport));
        BedrockLog::flush();
        return;
    }

    const int maxPlayers = properties.isLoaded() ? properties.getMaxPlayers() : settings.maxPlayers;
    const std::string serverName = properties.isLoaded() ? properties.getServerName() : settings.motd;

    ServerNetworkHandler networkHandler(serverName, settings.subMotd, maxPlayers, transport);
    networkHandler.setProtocolVersion(settings.protocolVersion, settings.gameVersion);
    networkHandler.setProperties(properties);

    if (properties.getAllowList() && networkHandler.getAllowList().isEmpty()) {
        LOG_WARN(LogAreaID::Server, "================ ALLOW LIST WARNING ===================");
        LOG_WARN(LogAreaID::Server, "Allow list is enabled but contains no entries. ");
        LOG_WARN(LogAreaID::Server, "Use allowlist add <playername> for you and your friends so that they can access "
                                    "the server, or modify allowlist.json manually.");
        LOG_WARN(LogAreaID::Server, "\nAlternatively, the allow list can be turned off by typing allowlist off or "
                                    "manually toggled in the server.properties file.");
        LOG_WARN(LogAreaID::Server, "=======================================================");
    }

    unsigned short port = properties.isLoaded() ? properties.getServerPort() : settings.port;
    unsigned short portV6 = properties.isLoaded() ? properties.getServerPortV6() : settings.portV6;

    // the environment variable stays available so a second instance can be started without editing the file
    if (const char *portOverride = getenv("FALCON_PORT"))
        port = (unsigned short) atoi(portOverride);

    const ConnectionDefinition definition = ConnectionDefinition::createFromPorts(port, portV6, maxPlayers);

    if (!networkHandler.startServerListening(definition)) {
        LOG_FATAL(LogAreaID::Server, "Failed to start server");
        BedrockLog::flush();
        return;
    }

    LOG_INFO(LogAreaID::Server, "Server started.");
    logTransportNotice(transport);
    logTelemetryNotice();
    BedrockLog::flush();

    VersionChecker::checkForUpdatesAsync();

    std::thread consoleThread([&networkHandler]() {
        std::string line;
        while (gRunning.load()) {
            if (!waitForConsoleInput(CONSOLE_POLL_INTERVAL))
                continue;

            if (!std::getline(std::cin, line))
                break;

            if (!line.empty())
                networkHandler.queueConsoleCommand(line);
        }
    });
    consoleThread.detach();

    std::signal(SIGINT, requestShutdown);
    std::signal(SIGTERM, requestShutdown);

    AutoCompaction::start(networkHandler.getLevel(), properties.getAutoCompactionInterval());

    const std::chrono::nanoseconds tickInterval(50000000);
    const std::chrono::nanoseconds catchupResetInterval(1000000000);
    std::chrono::steady_clock::time_point nextTick = std::chrono::steady_clock::now();

    while (gRunning.load() && !networkHandler.isStopRequested()) {
        networkHandler.tick();

        nextTick += tickInterval;

        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if (now - nextTick > catchupResetInterval)
            nextTick = now;
        else
            std::this_thread::sleep_until(nextTick);
    }

    LOG_INFO(LogAreaID::Server, "Shutting down...");
    AutoCompaction::stop();
    networkHandler.stopServerListening();
    BedrockLog::shutdown();
}

int main(int argc, char **argv) {
    ServerSettings settings;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];

        if (argument == "--no-wizard")
            settings.runSetupWizard = false;
        else if (argument == "--accept-license")
            settings.acceptLicense = true;
        else if (argument.rfind("--language=", 0) == 0)
            settings.language = argument.substr(std::string("--language=").size());
    }

    startServer(settings);
    return 0;
}
