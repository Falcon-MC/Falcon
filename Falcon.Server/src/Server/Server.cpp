#include "Server/Server.h"

#include "Server/VersionChecker.h"
#include "Falcon/Plugin/PluginEvent.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Debug/ContentLogEndPoint.h"
#include "Core/Debug/FileLogEndPoint.h"
#include "Network/ServerNetworkHandler.h"
#include "Resources/PackOptimizer.h"
#include "Server/ServerHost.h"
#include "Server/ServerTelemetry.h"
#include "Server/SetupWizard.h"

#ifdef __APPLE__
#include <pthread.h>
#include <pthread/qos.h>
#include <sched.h>
#endif

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
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

// blocking on std::cin holds the stdin lock and deadlocks the exit-time flush;
// waiting on the descriptor leaves the lock free until a line is ready
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

namespace {
    class ConsoleTerminal final : public ILogEndPoint {
    public:
        bool enable() {
#ifdef _WIN32
            mInput = GetStdHandle(STD_INPUT_HANDLE);
            mOutput = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD inputMode = 0;
            if (!GetConsoleMode(mInput, &inputMode) || !GetConsoleMode(mOutput, &mOutputMode)
                || !SetConsoleMode(mOutput, mOutputMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
                return false;
            mOutputCodePage = GetConsoleOutputCP();
            SetConsoleOutputCP(CP_UTF8);
            return true;
#else
            if (!isatty(STDOUT_FILENO) || tcgetattr(STDIN_FILENO, &mInputMode) != 0)
                return false;
            auto mode = mInputMode;
            mode.c_lflag &= ~(ICANON | ECHO);
            mode.c_cc[VMIN] = 1;
            mode.c_cc[VTIME] = 0;
            return tcsetattr(STDIN_FILENO, TCSANOW, &mode) == 0;
#endif
        }

        void log(const LogDetails &details) override {
            std::lock_guard<std::mutex> lock(mMutex);
            if (mEditing)
                _clear();
            mOutputLog.log(details);
            if (mEditing)
                _draw();
        }

        void flush() override {
            std::lock_guard<std::mutex> lock(mMutex);
            mOutputLog.flush();
        }

        void run(ServerNetworkHandler &handler, const std::shared_ptr<ConsoleTerminal> &self) {
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mEditing = true;
                _draw();
            }
            std::vector<std::string> history;
            std::string draft;
            size_t historyIndex = 0;
            while (gRunning.load()) {
                const int key = _readKey();
                if (key == NO_INPUT)
                    continue;
                std::lock_guard<std::mutex> lock(mMutex);
                if (key == INPUT_END || ((key == 4 || key == 26) && mLine.empty()))
                    break;
                if (key == '\t') {
                    const auto line = mLine;
                    const auto revision = mRevision;
                    handler.postToMainThread([&handler, self, line, revision]() {
                        std::vector<std::string> players;
                        for (const auto &[id, player]: handler.getPlayers()) {
                            if (player.isSpawned())
                                players.push_back(player.getName());
                        }
                        size_t start = 0;
                        const auto choices = handler.getCommands().complete(line, players, start);
                        self->_complete(revision, start, choices);
                    });
                    continue;
                }

                _clear();
                ++mRevision;
                if (key == '\r' || key == '\n') {
                    std::fputs(("> " + mLine + "\n").c_str(), stdout);
                    if (!mLine.empty()) {
                        if (history.empty() || history.back() != mLine)
                            history.push_back(mLine);
                        if (history.size() > 200)
                            history.erase(history.begin());
                        handler.queueConsoleCommand(mLine);
                    }
                    historyIndex = history.size();
                    mLine.clear();
                    draft.clear();
                } else if (key == KEY_UP && historyIndex > 0) {
                    if (historyIndex == history.size())
                        draft = mLine;
                    mLine = history[--historyIndex];
                } else if (key == KEY_DOWN && historyIndex < history.size()) {
                    ++historyIndex;
                    mLine = historyIndex == history.size() ? draft : history[historyIndex];
                } else if ((key == 8 || key == 127) && !mLine.empty()) {
                    do {
                        const unsigned char byte = (unsigned char) mLine.back();
                        mLine.pop_back();
                        if ((byte & 0xC0) != 0x80)
                            break;
                    } while (!mLine.empty());
                } else if (key == 21) {
                    mLine.clear();
                } else if (key >= 32 && key <= 255 && key != 127) {
                    mLine += (char) key;
                }
                _draw();
            }

            std::lock_guard<std::mutex> lock(mMutex);
            _clear();
            mEditing = false;
            mOutputLog.flush();
#ifdef _WIN32
            SetConsoleMode(mOutput, mOutputMode);
            if (mOutputCodePage != 0)
                SetConsoleOutputCP(mOutputCodePage);
#else
            tcsetattr(STDIN_FILENO, TCSANOW, &mInputMode);
#endif
        }

    private:
        static constexpr int NO_INPUT = -1;
        static constexpr int INPUT_END = -2;
        static constexpr int KEY_UP = 256;
        static constexpr int KEY_DOWN = 257;

        int _readKey() {
#ifdef _WIN32
            if (mPendingKey < mPendingSize)
                return (unsigned char) mPendingInput[mPendingKey++];
            const DWORD ready = WaitForSingleObject(mInput, CONSOLE_POLL_INTERVAL);
            if (ready != WAIT_OBJECT_0)
                return ready == WAIT_TIMEOUT ? NO_INPUT : INPUT_END;

            INPUT_RECORD record{};
            DWORD count = 0;
            if (!ReadConsoleInputW(mInput, &record, 1, &count))
                return INPUT_END;
            if (count == 0 || record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown)
                return NO_INPUT;

            const auto &event = record.Event.KeyEvent;
            if (event.wVirtualKeyCode == VK_UP)
                return KEY_UP;
            if (event.wVirtualKeyCode == VK_DOWN)
                return KEY_DOWN;
            const wchar_t key = event.uChar.UnicodeChar;
            if (key < 128)
                return key;

            wchar_t input[2] = {key, 0};
            int size = 1;
            if (key >= 0xD800 && key <= 0xDBFF) {
                mHighSurrogate = key;
                return NO_INPUT;
            }
            if (key >= 0xDC00 && key <= 0xDFFF && mHighSurrogate != 0) {
                input[0] = mHighSurrogate;
                input[1] = key;
                size = 2;
            }
            mHighSurrogate = 0;
            mPendingSize = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input, size, mPendingInput,
                                               sizeof(mPendingInput), nullptr, nullptr);
            mPendingKey = 0;
            return mPendingSize > 0 ? (unsigned char) mPendingInput[mPendingKey++] : NO_INPUT;
#else
            if (!waitForConsoleInput(CONSOLE_POLL_INTERVAL)) {
                mEscapeState = 0;
                return NO_INPUT;
            }

            unsigned char key;
            const auto count = read(STDIN_FILENO, &key, 1);
            if (count != 1)
                return count < 0 && errno == EINTR ? NO_INPUT : INPUT_END;
            if (key == 27) {
                mEscapeState = 1;
                return NO_INPUT;
            }
            if (mEscapeState == 1) {
                mEscapeState = (key == '[' || key == 'O') ? 2 : 0;
                return NO_INPUT;
            }
            if (mEscapeState == 2) {
                if (key < 0x40 || key > 0x7E)
                    return NO_INPUT;
                mEscapeState = 0;
                if (key == 'A')
                    return KEY_UP;
                if (key == 'B')
                    return KEY_DOWN;
                return NO_INPUT;
            }
            return key;
#endif
        }

        static void _clear() {
            std::fputs("\r\033[2K", stdout);
        }

        size_t _columns() const {
#ifdef _WIN32
            CONSOLE_SCREEN_BUFFER_INFO window{};
            if (GetConsoleScreenBufferInfo(mOutput, &window))
                return (size_t) (window.srWindow.Right - window.srWindow.Left + 1);
#else
            struct winsize window{};
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) == 0 && window.ws_col > 0)
                return window.ws_col;
#endif
            return 80;
        }

        void _draw() {
            const auto columns = _columns();
            if (columns < 5)
                return;
            size_t start = 0;
            if (mLine.size() > columns - 3) {
                start = mLine.size() - (columns - 4);
                while (start < mLine.size() && ((unsigned char) mLine[start] & 0xC0) == 0x80)
                    ++start;
            }
            std::fputs(start == 0 ? "> " : "> <", stdout);
            std::fputs(mLine.c_str() + start, stdout);
            mOutputLog.flush();
        }

        void _complete(size_t revision, size_t start, const std::vector<std::string> &choices) {
            std::lock_guard<std::mutex> lock(mMutex);
            if (!mEditing || revision != mRevision || choices.empty())
                return;

            _clear();
            if (choices.size() == 1) {
                mLine.replace(start, std::string::npos, choices.front() + " ");
                ++mRevision;
            } else {
                std::string common = choices.front();
                for (const auto &choice: choices) {
                    const auto prefixEnd = std::mismatch(common.begin(), common.end(), choice.begin(), choice.end());
                    common.erase(prefixEnd.first, common.end());
                }
                const auto &first = choices.front();
                while (!common.empty() && common.size() < first.size()
                       && ((unsigned char) first[common.size()] & 0xC0) == 0x80)
                    common.pop_back();
                if (common.size() > mLine.size() - start) {
                    mLine.replace(start, std::string::npos, common);
                    ++mRevision;
                } else {
                    const size_t shown = std::min<size_t>(choices.size(), 20);
                    std::string matches = choices.front();
                    for (size_t index = 1; index < shown; ++index)
                        matches += "  " + choices[index];
                    if (shown < choices.size())
                        matches += "  (" + std::to_string(choices.size() - shown) + " more)";
                    std::puts(matches.c_str());
                }
            }
            _draw();
        }

        std::mutex mMutex;
        ContentLogEndPoint mOutputLog;
        std::string mLine;
        size_t mRevision = 0;
        bool mEditing = false;
#ifdef _WIN32
        HANDLE mInput = nullptr;
        HANDLE mOutput = nullptr;
        DWORD mOutputMode = 0;
        UINT mOutputCodePage = 0;
        wchar_t mHighSurrogate = 0;
        char mPendingInput[4]{};
        int mPendingKey = 0;
        int mPendingSize = 0;
#else
        termios mInputMode{};
        int mEscapeState = 0;
#endif
    };
}

static bool fileExists(const char *path) {
    std::ifstream file(path);
    return file.is_open();
}

static void setupServerLogging(const std::shared_ptr<ILogEndPoint> &console = nullptr) {
    static const char *LOG_FILE = "server.log";

    if (!fileExists(LOG_FILE)) {
        printf("NO LOG FILE! - setting up server logging...\n");
        fflush(stdout);
    }

    BedrockLog::setLogLevel(LogLevel::Info);
    BedrockLog::addEndPoint(console != nullptr ? console : std::make_shared<ContentLogEndPoint>());

    std::shared_ptr<FileLogEndPoint> fileEndPoint = std::make_shared<FileLogEndPoint>(LOG_FILE);
    if (fileEndPoint->isOpen())
        BedrockLog::addEndPoint(fileEndPoint);
}

static void logTelemetryNotice(bool enabled) {
    LOG_INFO(LogAreaID::Server, "================ TELEMETRY MESSAGE ===================");
    if (enabled) {
        LOG_INFO(LogAreaID::Server, "Server Telemetry is enabled. Thank you for helping improve Falcon.");
        LOG_INFO(LogAreaID::Server, "%s", "");
        LOG_INFO(LogAreaID::Server, "Sent every half hour: version, system, transport, player count and limit,");
        LOG_INFO(LogAreaID::Server, "uptime, client check counts. No address, name or identifier is sent.");
        LOG_INFO(LogAreaID::Server, "To disable this feature, set 'emit-server-telemetry=false'");
        LOG_INFO(LogAreaID::Server, "======================================================");
        return;
    }
    LOG_INFO(LogAreaID::Server, "Server Telemetry is currently not enabled. ");
    LOG_INFO(LogAreaID::Server, "Enabling this telemetry helps us improve the game.");
    LOG_INFO(LogAreaID::Server, "%s", "");
    LOG_INFO(LogAreaID::Server, "To enable this feature, add the line 'emit-server-telemetry=true'");
    LOG_INFO(LogAreaID::Server, "to the server.properties file in the handheld/src-server directory");
    LOG_INFO(LogAreaID::Server, "======================================================");
}

void startServer(const ServerSettings &settings) {
    auto terminal = std::make_shared<ConsoleTerminal>();
    setupServerLogging(terminal);

    if (settings.runSetupWizard && SetupWizard::isInteractive() && SetupWizard::isNeeded(PROPERTIES_FILE)) {
        SetupWizard wizard(PROPERTIES_FILE, OPS_FILE, ALLOWLIST_FILE);
        if (!wizard.run(settings.acceptLicense, settings.language)) {
            BedrockLog::shutdown();
            return;
        }
    }

    ServerHostOptions options = settings.host;

    // kept as an env variable: a second instance starts without editing the file
    if (const char *portOverride = getenv("FALCON_PORT"))
        options.portOverride = (unsigned short) atoi(portOverride);

    ServerHost host;
    if (!host.start(options))
        return;

    const PropertiesSettings &properties = host.getHandler().getProperties();
    const bool telemetry = properties.getEmitServerTelemetry();
    logTelemetryNotice(telemetry);
    BedrockLog::flush();
    if (telemetry)
        ServerTelemetry::getInstance().start(properties.getTransportLayer() == TransportLayer::NetherNet ? "nethernet"
                                                                                                         : "raknet",
                                             properties.getMaxPlayers());

    VersionChecker::checkForUpdatesAsync();

    ServerNetworkHandler &networkHandler = host.getHandler();
    std::signal(SIGINT, requestShutdown);
    std::signal(SIGTERM, requestShutdown);

    const bool interactive = terminal->enable();
    std::thread consoleThread([&networkHandler, terminal, interactive]() {
        if (interactive) {
            terminal->run(networkHandler, terminal);
            return;
        }
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
    if (!interactive)
        consoleThread.detach();

    const std::chrono::nanoseconds tickInterval(50000000);
    const std::chrono::nanoseconds catchupResetInterval(1000000000);
    std::chrono::steady_clock::time_point nextTick = std::chrono::steady_clock::now();

    // an error out of a tick still ends with the shutdown save: players and the containers they used stay together
#ifdef __APPLE__
    // the tick thread ranks with the threads a user is waiting on: background work on the machine does not
    // push it off its core in the middle of a tick
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
    // a server started from a shell or a service is no application: the class alone leaves the thread at the
    // default priority, where its own workers and every other process outrank it once it has run for a while
    sched_param tickPriority;
    int tickPolicy = 0;
    if (pthread_getschedparam(pthread_self(), &tickPolicy, &tickPriority) == 0) {
        tickPriority.sched_priority = sched_get_priority_max(tickPolicy);
        pthread_setschedparam(pthread_self(), tickPolicy, &tickPriority);
    }
#endif

    bool failed = false;
    while (gRunning.load() && !host.isStopRequested()) {
        try {
            if (PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_UPDATE_NOTIFY)) {
                std::string release = VersionChecker::takeAvailableUpdate();
                if (!release.empty()) {
                    PluginEvent event;
                    event.mType = FALCON_EVENT_UPDATE_NOTIFY;
                    event.mCustomData = std::move(release);
                    plugins->dispatch(event);
                }
            }
            host.tick();
        } catch (const std::exception &error) {
            LOG_ERROR(LogAreaID::Server, "The server stopped on an error: %s", error.what());
            failed = true;
            break;
        } catch (...) {
            LOG_ERROR(LogAreaID::Server, "The server stopped on an unknown error");
            failed = true;
            break;
        }

        nextTick += tickInterval;

        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if (now - nextTick > catchupResetInterval)
            nextTick = now;
        else
            host.waitForNextTick(nextTick);
    }

    gRunning.store(false);
    if (consoleThread.joinable())
        consoleThread.join();
    host.shutdown();
    ServerTelemetry::getInstance().stop();
    BedrockLog::shutdown();

    // saved by now; on Windows the console thread can sit in std::getline holding the stdin lock, and a normal exit
    // flushes stdin too: it hangs; worker and plugin host threads block destructors alike: _Exit skips them
    std::cout.flush();
    std::fflush(nullptr);
    std::_Exit(failed ? 1 : 0);
}

int main(int argc, char **argv) {
    ServerSettings settings;
    std::string packOptimizerConfig;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];

        if (argument.rfind(PackOptimizer::ARGUMENT, 0) == 0)
            packOptimizerConfig = argument.substr(std::string(PackOptimizer::ARGUMENT).size());
        else if (argument == "--no-wizard")
            settings.runSetupWizard = false;
        else if (argument == "--accept-license")
            settings.acceptLicense = true;
        else if (argument.rfind("--language=", 0) == 0)
            settings.language = argument.substr(std::string("--language=").size());
    }

    // the game runs the optimizer instead of the server and exits with its code
    if (!packOptimizerConfig.empty()) {
        setupServerLogging();
        const int code = PackOptimizer::run(packOptimizerConfig);
        BedrockLog::shutdown();
        return code;
    }

    startServer(settings);
    return 0;
}
