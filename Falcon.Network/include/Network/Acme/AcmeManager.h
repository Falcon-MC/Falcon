#pragma once

#include "Network/Acme/AcmeChallengeStore.h"
#include "Network/Acme/AcmeClient.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace acme {

    struct ManagerSettings {
        ClientSettings mClient;
        // one owner-only folder per directory URL: account key, key and chain bundle, retry state
        std::string mStorageDirectory;
    };

    // keeps one certificate issued and renewed; never blocks its caller past reading the stored files
    class Manager {
    public:
        typedef std::function<bool(const std::string &certificatePath, const std::string &keyPath)> Installer;

        // first retry after a failure, doubled up to a day
        static constexpr int64_t FIRST_RETRY_SECONDS = 600;
        static constexpr int64_t MAX_RETRY_SECONDS = 24 * 3600;
        // floor after a rate limit refusal without Retry-After
        static constexpr int64_t RATE_LIMIT_RETRY_SECONDS = 3600;

        Manager(const ManagerSettings &settings, std::shared_ptr<ChallengeStore> challenges, Installer installer);

        ~Manager();

        Manager(const Manager &) = delete;

        Manager &operator=(const Manager &) = delete;

        // false when the domain or directory is unusable; nothing is written then
        bool start();

        void stop();

        std::string getCertificatePath() const;

        std::string getKeyPath() const;

        static int64_t retryDelay(int failures, bool rateLimited, int64_t retryAfterSeconds);

    private:
        void _run();

        void _attempt();

        bool _installStored(int64_t &outRenewAt);

        std::shared_ptr<EcKey> _accountKey();

        void _readState();

        void _writeState();

        std::string _directoryFolder() const;

        std::string _path(const std::string &name) const;

        ManagerSettings mSettings;
        std::shared_ptr<ChallengeStore> mChallenges;
        Installer mInstaller;

        std::thread mThread;
        std::mutex mMutex;
        std::condition_variable mWake;
        std::atomic<bool> mStopping{false};
        bool mRunning = false;

        // unix seconds; persisted so a restart loop cannot hammer the CA
        int64_t mNextAttempt = 0;
        int mFailures = 0;
        int64_t mInstalledNotAfter = 0;
    };

}
