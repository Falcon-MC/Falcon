#include "Server.h"

#include "Server/VersionChecker.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Debug/ContentLogEndPoint.h"
#include "Core/Debug/FileLogEndPoint.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Server/ServerHost.h"
#include "Server/SetupWizard.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <thread>

#ifndef _WIN32
#include <poll.h>
#include <unistd.h>
#endif

static const char *PROPERTIES_FILE = "server.properties";
static const char *OPS_FILE = "ops.txt";
static const char *ALLOWLIST_FILE = "allowlist.json";

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

static bool fileExists(const char *path) {
    std::ifstream file(path);
    return file.is_open();
}

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

    if (settings.runSetupWizard && SetupWizard::isInteractive() && SetupWizard::isNeeded(PROPERTIES_FILE)) {
        SetupWizard wizard(PROPERTIES_FILE, OPS_FILE, ALLOWLIST_FILE);
        if (!wizard.run(settings.acceptLicense, settings.language)) {
            BedrockLog::shutdown();
            return;
        }
    }

    ServerHostOptions options = settings.host;

    // the environment variable stays available so a second instance can be started without editing the file
    if (const char *portOverride = getenv("FALCON_PORT"))
        options.portOverride = (unsigned short) atoi(portOverride);

    ServerHost host;
    if (!host.start(options))
        return;

    logTelemetryNotice();
    BedrockLog::flush();

    VersionChecker::checkForUpdatesAsync();

    ServerNetworkHandler &networkHandler = host.getHandler();
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

    const std::chrono::nanoseconds tickInterval(50000000);
    const std::chrono::nanoseconds catchupResetInterval(1000000000);
    std::chrono::steady_clock::time_point nextTick = std::chrono::steady_clock::now();

    while (gRunning.load() && !host.isStopRequested()) {
        host.tick();

        nextTick += tickInterval;

        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if (now - nextTick > catchupResetInterval)
            nextTick = now;
        else
            std::this_thread::sleep_until(nextTick);
    }

    gRunning.store(false);
    host.shutdown();
    BedrockLog::shutdown();

    // The worlds and player data are saved by now. On Windows the detached console thread may still be blocked
    // inside std::getline holding the stdin lock (a pipe that never closes never returns), and a normal exit
    // flushes every stream, stdin included, so it would wait on that lock forever. Leftover worker and plugin
    // host threads can block destructors the same way, so the process ends here without running them.
    std::cout.flush();
    std::fflush(nullptr);
    std::_Exit(0);
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
