#include "Network/Http/ServiceError.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

ServiceErrorKind ServiceError::classify(int status) {
    if (status >= 200 && status < 300)
        return ServiceErrorKind::None;

    switch (status) {
        case 401:
            return ServiceErrorKind::Unauthorized;
        case 403:
            return ServiceErrorKind::Forbidden;
        case 404:
            return ServiceErrorKind::NotFound;
        case 409:
        case 410:
            return ServiceErrorKind::Conflict;
        case 429:
            return ServiceErrorKind::RateLimited;
        case 408:
        case 504:
            return ServiceErrorKind::Timeout;
        default:
            break;
    }

    return status >= 500 ? ServiceErrorKind::Unavailable : ServiceErrorKind::Failed;
}

ServiceErrorKind ServiceError::classifyTransport(const std::string &error, bool cancelled) {
    if (cancelled)
        return ServiceErrorKind::Cancelled;

    return error.find("timed out") != std::string::npos ? ServiceErrorKind::Timeout : ServiceErrorKind::Network;
}

const char *ServiceError::name(ServiceErrorKind kind) {
    switch (kind) {
        case ServiceErrorKind::None:
            return "none";
        case ServiceErrorKind::Unauthorized:
            return "unauthorized";
        case ServiceErrorKind::Forbidden:
            return "forbidden";
        case ServiceErrorKind::NotFound:
            return "not_found";
        case ServiceErrorKind::Conflict:
            return "conflict";
        case ServiceErrorKind::RateLimited:
            return "rate_limited";
        case ServiceErrorKind::Unavailable:
            return "unavailable";
        case ServiceErrorKind::Timeout:
            return "timeout";
        case ServiceErrorKind::Cancelled:
            return "cancelled";
        case ServiceErrorKind::Network:
            return "network";
        case ServiceErrorKind::InvalidResponse:
            return "invalid_response";
        case ServiceErrorKind::Failed:
            return "failed";
    }

    return "failed";
}

int ServiceError::parseRetryAfter(const std::string &header, int maximumMs) {
    if (header.empty() || !std::all_of(header.begin(), header.end(), [](char c) {
            return std::isdigit((unsigned char) c) != 0;
        }))
        return 0;

    const long seconds = std::strtol(header.c_str(), nullptr, 10);
    if (seconds <= 0)
        return 0;

    return (int) std::min<long long>((long long) seconds * 1000, maximumMs);
}
