#pragma once

#include <string>

enum class ServiceErrorKind {
    None,
    Unauthorized,
    Forbidden,
    NotFound,
    Conflict,
    RateLimited,
    Unavailable,
    Timeout,
    Cancelled,
    Network,
    InvalidResponse,
    Failed,
};

/**
 * Why a call to an online service failed, sorted into what a caller can act
 * on: sign in again, give up, wait and retry, or report the service down.
 */
struct ServiceError {
    ServiceErrorKind mKind = ServiceErrorKind::None;
    int mStatus = 0;
    int mRetryAfterMs = 0;

    bool failed() const {
        return mKind != ServiceErrorKind::None;
    }

    void clear() {
        mKind = ServiceErrorKind::None;
        mStatus = 0;
        mRetryAfterMs = 0;
    }

    static ServiceErrorKind classify(int status);

    static ServiceErrorKind classifyTransport(const std::string &error, bool cancelled);

    static const char *name(ServiceErrorKind kind);

    static int parseRetryAfter(const std::string &header, int maximumMs);
};
