#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class NetStream;

namespace HttpsClient {

    const int DEFAULT_TIMEOUT_SECONDS = 5;

    // https only, a 200 answer only
    bool get(const std::string &url, std::string &body, int timeoutSeconds = DEFAULT_TIMEOUT_SECONDS);

    using Headers = std::vector<std::pair<std::string, std::string>>;

    struct Request {
        std::string mMethod = "GET";
        // http or https
        std::string mUrl;
        Headers mHeaders;
        std::string mBody;
        // each connect, write and read; 0 waits forever
        uint32_t mTimeoutSeconds = 0;
        // 0: no limit
        size_t mMaxResponseBytes = 0;
    };

    struct Response {
        // false: no answer, mErrorCode and mErrorMessage tell why
        bool mCompleted = false;
        int mStatus = 0;
        Headers mHeaders;
        std::string mBody;
        int mErrorCode = 0;
        std::string mErrorMessage;
    };

    // blocking; stream lets another thread abort the exchange
    Response perform(const Request &request, NetStream &stream);

    // reads one head up to the blank line; extra bytes after it stay in rest
    bool readHead(NetStream &stream, std::string &head, std::string &rest, size_t limit);

    // the status line and fields of a response head
    bool parseHead(const std::string &head, int &status, Headers &headers);

}
