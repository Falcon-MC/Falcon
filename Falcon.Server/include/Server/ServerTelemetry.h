#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>

// reports how the server is doing to the Falcon service, only when emit-server-telemetry is on. A report
// holds the server version, system, architecture, transport, player count, player limit, uptime, how
// long ticks took, how often each client check fired and why connections failed: nothing that names
// the server, its world or a player
class ServerTelemetry {
public:
    static ServerTelemetry &getInstance();

    // sends a first report, then one every half hour
    void start(const std::string &transport, int maxPlayers);

    // sends a last report, without holding the shutdown for long
    void stop();

    bool isRunning() const {
        return mRunning.load();
    }

    void setPlayers(int players) {
        mPlayers.store(players);
    }

    // counted whether or not telemetry is on: asking costs more than counting
    void countCheck(int code);

    // a connection that did not make it into the world: where it failed and the reason's number there
    enum class FailureStage : char { Offer = 'n', OfferCheck = 'c', Login = 'd' };

    void countFailure(FailureStage stage, int reason);

    void recordTick(double milliseconds);

private:
    ServerTelemetry() = default;

    void _run();

    void _send(const char *event, int timeoutMs);

    std::atomic<bool> mRunning{false};
    std::atomic<int> mPlayers{0};
    std::mutex mMutex;
    std::condition_variable mWake;
    std::map<int, long long> mChecks;
    std::map<std::string, long long> mFailures;
    std::atomic<long long> mTickTotalUs{0};
    std::atomic<long long> mTickMaxUs{0};
    std::atomic<long long> mTicks{0};
    std::thread mWorker;
    bool mStopping = false;
    std::string mTransport;
    int mMaxPlayers = 0;
    std::chrono::steady_clock::time_point mStarted;
};
