#include "Server/ServerTelemetry.h"

#include "BuildInfo.h"
#include "Network/Http/HttpClient.h"

namespace {
    const char *const SERVICE = "https://api.falcon-mc.workers.dev/v1/telemetry";

    const std::chrono::minutes REPORT_INTERVAL(30);
    const int REPORT_TIMEOUT_MS = 5000;
    // a server that is closing does not wait on a report
    const int LAST_REPORT_TIMEOUT_MS = 1500;
    // the service refuses a report naming more checks than this
    const size_t MAX_CHECKS = 32;
    const size_t MAX_FAILURES = 48;
    const size_t MAX_REFUSALS = 64;

#if defined(_WIN32)
    const char *const SYSTEM = "windows";
#elif defined(__APPLE__)
    const char *const SYSTEM = "macos";
#else
    const char *const SYSTEM = "linux";
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
    const char *const ARCHITECTURE = "aarch64";
#else
    const char *const ARCHITECTURE = "x86_64";
#endif
}

ServerTelemetry &ServerTelemetry::getInstance() {
    static ServerTelemetry instance;
    return instance;
}

void ServerTelemetry::start(const std::string &transport, int maxPlayers) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mRunning.load())
        return;

    mTransport = transport;
    mMaxPlayers = maxPlayers < 0 ? 0 : maxPlayers;
    mStarted = std::chrono::steady_clock::now();
    mStopping = false;
    mRunning.store(true);
    mWorker = std::thread([this]() { _run(); });
}

void ServerTelemetry::stop() {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mRunning.load())
            return;
        mStopping = true;
    }
    mWake.notify_all();
    if (mWorker.joinable())
        mWorker.join();

    _send("stop", LAST_REPORT_TIMEOUT_MS);
    mRunning.store(false);
}

void ServerTelemetry::countCheck(int code) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (code > 0 && code <= 9999 && (mChecks.count(code) != 0 || mChecks.size() < MAX_CHECKS))
        mChecks[code]++;
}

void ServerTelemetry::countFailure(FailureStage stage, int reason) {
    if (reason < 0 || reason > 9999)
        return;

    const std::string key = std::string(1, (char) stage) + std::to_string(reason);
    std::lock_guard<std::mutex> lock(mMutex);
    if (mFailures.count(key) != 0 || mFailures.size() < MAX_FAILURES)
        mFailures[key]++;
}

void ServerTelemetry::countRefusal(int code, int platform) {
    if (code <= 0 || code > 9999)
        return;
    if (platform < 0 || platform > 99)
        platform = 0;

    const std::string key = std::to_string(code) + ":" + std::to_string(platform);
    std::lock_guard<std::mutex> lock(mMutex);
    if (mRefusals.count(key) != 0 || mRefusals.size() < MAX_REFUSALS)
        mRefusals[key]++;
}

void ServerTelemetry::recordTick(double milliseconds) {
    if (!(milliseconds >= 0.0))
        return;

    // capped at a minute: beyond that the service takes the report for a mistake
    const long long microseconds = milliseconds > 60000.0 ? 60000000 : (long long) (milliseconds * 1000.0);
    mTickTotalUs.fetch_add(microseconds);
    mTicks.fetch_add(1);
    long long highest = mTickMaxUs.load();
    while (microseconds > highest && !mTickMaxUs.compare_exchange_weak(highest, microseconds)) {
    }
}

void ServerTelemetry::_run() {
    _send("start", REPORT_TIMEOUT_MS);

    for (;;) {
        {
            std::unique_lock<std::mutex> lock(mMutex);
            if (mWake.wait_for(lock, REPORT_INTERVAL, [this]() { return mStopping; }))
                return;
        }
        _send("heartbeat", REPORT_TIMEOUT_MS);
    }
}

void ServerTelemetry::_send(const char *event, int timeoutMs) {
    std::map<int, long long> checks;
    std::map<std::string, long long> failures;
    std::map<std::string, long long> refusals;
    std::string transport;
    int maxPlayers = 0;
    long long uptime = 0;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        checks.swap(mChecks);
        failures.swap(mFailures);
        refusals.swap(mRefusals);
        transport = mTransport;
        maxPlayers = mMaxPlayers;
        uptime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - mStarted).count();
    }

    // every value is a number or one of a few fixed words: nothing here needs escaping
    std::string report = "{\"event\":\"";
    report += event;
    report += "\",\"version\":\"";
    report += FalconBuildInfo::kVersion;
    report += "\",\"os\":\"";
    report += SYSTEM;
    report += "\",\"arch\":\"";
    report += ARCHITECTURE;
    report += "\",\"transport\":\"";
    report += transport;
    report += "\",\"players\":" + std::to_string(mPlayers.load() < 0 ? 0 : mPlayers.load());
    report += ",\"maxPlayers\":" + std::to_string(maxPlayers);
    report += ",\"uptime\":" + std::to_string(uptime);
    report += ",\"checks\":{";
    bool first = true;
    for (const auto &check: checks) {
        if (!first)
            report.push_back(',');
        first = false;
        report += "\"" + std::to_string(check.first) + "\":" + std::to_string(check.second);
    }
    report += "},\"failures\":{";
    first = true;
    for (const auto &failure: failures) {
        if (!first)
            report.push_back(',');
        first = false;
        report += "\"" + failure.first + "\":" + std::to_string(failure.second);
    }
    report += "},\"refusals\":{";
    first = true;
    for (const auto &refusal: refusals) {
        if (!first)
            report.push_back(',');
        first = false;
        report += "\"" + refusal.first + "\":" + std::to_string(refusal.second);
    }
    report += "}";

    const long long ticks = mTicks.exchange(0);
    const long long total = mTickTotalUs.exchange(0);
    const long long highest = mTickMaxUs.exchange(0);
    if (ticks > 0) {
        report += ",\"tickAvgUs\":" + std::to_string(total / ticks);
        report += ",\"tickMaxUs\":" + std::to_string(highest);
    }
    report += "}";

    // a report that does not arrive is lost: telemetry never retries and never complains
    HttpResponse response;
    std::string error;
    HttpClient::post(SERVICE, {{"Content-Type", "application/json"}}, report, response, error, timeoutMs);
}
