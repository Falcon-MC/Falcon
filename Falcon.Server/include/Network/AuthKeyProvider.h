#pragma once

#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>

typedef struct evp_pkey_st EVP_PKEY;

class AuthKeyProvider {
public:
    static AuthKeyProvider &getInstance();

    ~AuthKeyProvider();

    void start();

    void stop();

    EVP_PKEY *acquireKey(const std::string &keyId, std::string &issuer);

    // true until the first fetch ends, failed or not: a login checked before then misses its key
    bool isFetchingFirstKeys();

private:
    AuthKeyProvider() = default;

    AuthKeyProvider(const AuthKeyProvider &) = delete;

    AuthKeyProvider &operator=(const AuthKeyProvider &) = delete;

    void _worker();

    bool _fetch(std::map<std::string, EVP_PKEY *> &outKeys, std::string &outIssuer);

    void _requestRefresh();

    void _clear();

    std::mutex mMutex;
    std::condition_variable mCondition;
    std::thread mWorker;
    bool mRunning = false;
    bool mRefreshRequested = false;
    std::map<std::string, EVP_PKEY *> mKeys;
    std::string mIssuer;
    long long mLastFetch = 0;
    long long mLastFailure = 0;
};
