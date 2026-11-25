#include "Network/Acme/AcmeManager.h"

#include "Core/Debug/BedrockLog.h"

#ifdef _WIN32
#include <windows.h>
#include <aclapi.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <vector>

namespace acme {

    namespace {

        const char *const ACCOUNT_KEY_FILE = "account.key";
        // a clock jump or a sleep must not push the next check past this
        const int64_t MAX_WAIT_SECONDS = 3600;

        int64_t now() {
            return (int64_t) std::time(nullptr);
        }

        std::string formatTime(int64_t seconds) {
            const std::time_t value = (std::time_t) seconds;
            char text[32];
            std::tm parts;
#ifdef _WIN32
            gmtime_s(&parts, &value);
#else
            gmtime_r(&value, &parts);
#endif
            std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M UTC", &parts);
            return text;
        }

        bool readFile(const std::string &path, std::string &outText) {
            std::ifstream file(path, std::ios::binary);
            if (!file.is_open())
                return false;
            std::ostringstream text;
            text << file.rdbuf();
            outText = text.str();
            return true;
        }

#ifdef _WIN32
        // a protected DACL granting the current account alone: the server folder's inherited ACL may be wider
        bool restrictToOwner(const std::string &path) {
            HANDLE token = nullptr;
            if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
                return false;

            DWORD length = 0;
            GetTokenInformation(token, TokenUser, nullptr, 0, &length);
            std::vector<unsigned char> buffer(length);
            const bool queried = length != 0
                                 && GetTokenInformation(token, TokenUser, buffer.data(), length, &length) != 0;
            CloseHandle(token);
            if (!queried)
                return false;

            EXPLICIT_ACCESSA access;
            ZeroMemory(&access, sizeof(access));
            access.grfAccessPermissions = GENERIC_ALL;
            access.grfAccessMode = SET_ACCESS;
            access.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
            access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
            access.Trustee.TrusteeType = TRUSTEE_IS_USER;
            access.Trustee.ptstrName = (LPSTR) ((TOKEN_USER *) buffer.data())->User.Sid;

            PACL acl = nullptr;
            if (SetEntriesInAclA(1, &access, nullptr, &acl) != ERROR_SUCCESS)
                return false;

            const DWORD result = SetNamedSecurityInfoA((LPSTR) path.c_str(), SE_FILE_OBJECT,
                                                       DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                                                       nullptr, nullptr, acl, nullptr);
            LocalFree(acl);
            return result == ERROR_SUCCESS;
        }
#endif

        // written beside then renamed: a crash never leaves half a file; secrets are owner-only before any byte
        bool writeFile(const std::string &path, const std::string &text, bool secret) {
            const std::string temporary = path + ".tmp";
#ifdef _WIN32
            {
                std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
                if (!file.is_open())
                    return false;
            }
            if (secret && !restrictToOwner(temporary))
                return false;
            {
                std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
                file << text;
                if (!file.good())
                    return false;
            }
#else
            const int descriptor = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC, secret ? 0600 : 0644);
            if (descriptor < 0)
                return false;
            if (secret)
                ::fchmod(descriptor, 0600);
            size_t written = 0;
            while (written < text.size()) {
                const ssize_t count = ::write(descriptor, text.data() + written, text.size() - written);
                if (count <= 0) {
                    ::close(descriptor);
                    return false;
                }
                written += (size_t) count;
            }
            ::fsync(descriptor);
            ::close(descriptor);
#endif
            std::error_code error;
            std::filesystem::rename(temporary, path, error);
            return !error;
        }
    }

    Manager::Manager(const ManagerSettings &settings, std::shared_ptr<ChallengeStore> challenges,
                     Installer installer)
            : mSettings(settings), mChallenges(std::move(challenges)), mInstaller(std::move(installer)) {
    }

    Manager::~Manager() {
        stop();
    }

    // one folder per directory URL: a staging certificate or back-off never carries over to production
    std::string Manager::_directoryFolder() const {
        const std::string digest = sha256(mSettings.mClient.mDirectoryUrl);
        static const char DIGITS[] = "0123456789abcdef";
        std::string name;
        for (size_t index = 0; index < 8 && index < digest.size(); ++index) {
            name.push_back(DIGITS[((unsigned char) digest[index]) >> 4]);
            name.push_back(DIGITS[((unsigned char) digest[index]) & 0x0f]);
        }
        return (std::filesystem::path(mSettings.mStorageDirectory) / name).string();
    }

    std::string Manager::_path(const std::string &name) const {
        return (std::filesystem::path(_directoryFolder()) / name).string();
    }

    // key and chain in one file: a single rename replaces both, they can never disagree
    std::string Manager::getCertificatePath() const {
        return _path(mSettings.mClient.mDomain + ".pem");
    }

    std::string Manager::getKeyPath() const {
        return getCertificatePath();
    }

    int64_t Manager::retryDelay(int failures, bool rateLimited, int64_t retryAfterSeconds) {
        retryAfterSeconds = std::min(std::max<int64_t>(retryAfterSeconds, 0), MAX_RETRY_SECONDS);
        if (rateLimited)
            return std::max(retryAfterSeconds, RATE_LIMIT_RETRY_SECONDS);

        int64_t delay = FIRST_RETRY_SECONDS;
        for (int failure = 1; failure < failures && delay < MAX_RETRY_SECONDS; ++failure)
            delay *= 2;
        return std::max(std::min(delay, MAX_RETRY_SECONDS), retryAfterSeconds);
    }

    bool Manager::start() {
        if (mRunning)
            return true;

        // the name becomes a file name: checked before any path is built
        mSettings.mClient.mDomain = normalizeDomain(mSettings.mClient.mDomain);
        if (!isDomain(mSettings.mClient.mDomain)) {
            LOG_ERROR(LogAreaID::Network, "ACME needs a public domain name, %s is not one",
                      mSettings.mClient.mDomain.c_str());
            return false;
        }
        if (mSettings.mClient.mDirectoryUrl.rfind("https://", 0) != 0) {
            LOG_ERROR(LogAreaID::Network, "The ACME directory must be an https:// URL, not %s",
                      mSettings.mClient.mDirectoryUrl.c_str());
            return false;
        }

        std::error_code error;
        std::filesystem::create_directories(_directoryFolder(), error);
#ifdef _WIN32
        restrictToOwner(mSettings.mStorageDirectory);
        restrictToOwner(_directoryFolder());
#else
        ::chmod(mSettings.mStorageDirectory.c_str(), 0700);
        ::chmod(_directoryFolder().c_str(), 0700);
#endif
        writeFile(_path("directory.txt"), mSettings.mClient.mDirectoryUrl + "\n", false);

        // a stored certificate is served before the first join, the network round trips wait for the thread
        int64_t renewAt = now();
        if (_installStored(renewAt))
            LOG_INFO(LogAreaID::Network, "ACME certificate for %s loaded, renewal from %s",
                     mSettings.mClient.mDomain.c_str(), formatTime(renewAt).c_str());

        _readState();
        mNextAttempt = std::max(mNextAttempt, renewAt);

        mStopping.store(false);
        mRunning = true;
        mThread = std::thread(&Manager::_run, this);
        return true;
    }

    void Manager::stop() {
        if (!mRunning)
            return;

        {
            std::lock_guard<std::mutex> lock(mMutex);
            mStopping.store(true);
        }
        mWake.notify_all();

        if (mThread.joinable())
            mThread.join();
        mRunning = false;
    }

    bool Manager::_installStored(int64_t &outRenewAt) {
        std::string bundle;
        if (!readFile(getCertificatePath(), bundle))
            return false;

        int64_t notBefore = 0;
        int64_t notAfter = 0;
        const std::shared_ptr<EcKey> key = EcKey::fromPem(bundle);
        if (key == nullptr || !readValidity(bundle, notBefore, notAfter) || now() >= notAfter
            || !certificateCovers(bundle, mSettings.mClient.mDomain) || !keyMatches(bundle, *key))
            return false;

        if (!mInstaller(getCertificatePath(), getKeyPath()))
            return false;

        mInstalledNotAfter = notAfter;
        outRenewAt = renewalTime(notBefore, notAfter);
        return true;
    }

    std::shared_ptr<EcKey> Manager::_accountKey() {
        std::string pem;
        if (readFile(_path(ACCOUNT_KEY_FILE), pem)) {
            std::shared_ptr<EcKey> stored = EcKey::fromPem(pem);
            if (stored != nullptr)
                return stored;
            LOG_WARN(LogAreaID::Network, "ACME account key %s is unreadable, creating a new account",
                     _path(ACCOUNT_KEY_FILE).c_str());
        }

        std::shared_ptr<EcKey> key = EcKey::generate();
        if (key == nullptr || !writeFile(_path(ACCOUNT_KEY_FILE), key->toPem(), true))
            return nullptr;
        return key;
    }

    void Manager::_readState() {
        std::string text;
        if (!readFile(_path(mSettings.mClient.mDomain + ".state"), text))
            return;

        std::istringstream lines(text);
        std::string line;
        while (std::getline(lines, line)) {
            const size_t separator = line.find('=');
            if (separator == std::string::npos)
                continue;
            const std::string name = line.substr(0, separator);
            const long long value = std::strtoll(line.c_str() + separator + 1, nullptr, 10);
            if (name == "next-attempt")
                mNextAttempt = (int64_t) value;
            else if (name == "failures")
                mFailures = (int) std::max(0LL, value);
        }
    }

    void Manager::_writeState() {
        const std::string text = "next-attempt=" + std::to_string(mNextAttempt) + "\nfailures="
                                 + std::to_string(mFailures) + "\n";
        if (!writeFile(_path(mSettings.mClient.mDomain + ".state"), text, false))
            LOG_WARN(LogAreaID::Network, "Could not save the ACME retry state in %s",
                     mSettings.mStorageDirectory.c_str());
    }

    void Manager::_run() {
        std::unique_lock<std::mutex> lock(mMutex);
        while (!mStopping.load()) {
            const int64_t remaining = mNextAttempt - now();
            if (remaining > 0) {
                mWake.wait_for(lock, std::chrono::seconds(std::min(remaining, MAX_WAIT_SECONDS)));
                continue;
            }

            lock.unlock();
            _attempt();
            lock.lock();
        }
    }

    void Manager::_attempt() {
        const std::string &domain = mSettings.mClient.mDomain;
        LOG_INFO(LogAreaID::Network, "Requesting a certificate for %s from %s (%s)", domain.c_str(),
                 mSettings.mClient.mDirectoryUrl.c_str(), toString(mSettings.mClient.mChallenge));

        Outcome outcome;
        const std::shared_ptr<EcKey> accountKey = _accountKey();
        const std::shared_ptr<EcKey> certificateKey = EcKey::generate();
        if (accountKey == nullptr || certificateKey == nullptr) {
            outcome.mError = "could not create or save the ACME keys in " + mSettings.mStorageDirectory;
        } else {
            Client client(mSettings.mClient, accountKey, *mChallenges, &mStopping);
            outcome = client.issue(*certificateKey);
        }

        if (mStopping.load())
            return;

        int64_t notBefore = 0;
        int64_t notAfter = 0;
        if (outcome.mIssued && readValidity(outcome.mChainPem, notBefore, notAfter)
            && keyMatches(outcome.mChainPem, *certificateKey)
            && writeFile(getCertificatePath(), certificateKey->toPem() + outcome.mChainPem, true)
            && mInstaller(getCertificatePath(), getKeyPath())) {
            mInstalledNotAfter = notAfter;
            mFailures = 0;
            mNextAttempt = renewalTime(notBefore, notAfter);
            _writeState();
            LOG_INFO(LogAreaID::Network, "ACME certificate for %s installed, valid until %s, renewal from %s",
                     domain.c_str(), formatTime(notAfter).c_str(), formatTime(mNextAttempt).c_str());
            return;
        }

        if (outcome.mIssued)
            outcome.mError = "the issued certificate could not be saved in " + mSettings.mStorageDirectory;

        ++mFailures;
        mNextAttempt = now() + retryDelay(mFailures, outcome.mRateLimited, outcome.mRetryAfterSeconds);
        _writeState();

        const bool stillServing = mInstalledNotAfter > now();
        LOG_WARN(LogAreaID::Network, "ACME certificate for %s failed: %s", domain.c_str(), outcome.mError.c_str());
        if (stillServing)
            LOG_WARN(LogAreaID::Network, "Keeping the current certificate (valid until %s); next try %s",
                     formatTime(mInstalledNotAfter).c_str(), formatTime(mNextAttempt).c_str());
        else
            LOG_WARN(LogAreaID::Network, "Joins keep using plain HTTP; next try %s",
                     formatTime(mNextAttempt).c_str());
    }

}
