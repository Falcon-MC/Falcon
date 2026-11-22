#include "Core/Net/HttpsClient.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/Net/NetStream.h"
#include "Core/Net/NetUri.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace {
    const size_t MAX_GET_RESPONSE_SIZE = 1024 * 1024;
    const size_t MAX_HEAD_SIZE = 64 * 1024;

    std::string lower(std::string text) {
        for (char &c: text)
            c = (char) std::tolower((unsigned char) c);
        return text;
    }

    std::string trim(const std::string &text) {
        size_t start = 0;
        size_t end = text.size();
        while (start < end && (text[start] == ' ' || text[start] == '\t'))
            ++start;
        while (end > start && (text[end - 1] == ' ' || text[end - 1] == '\t'))
            --end;
        return text.substr(start, end - start);
    }

    const std::string *findHeader(const HttpsClient::Headers &headers, const std::string &name) {
        for (const auto &header: headers) {
            if (lower(header.first) == name)
                return &header.second;
        }
        return nullptr;
    }

    bool fill(NetStream &stream, std::string &buffer) {
        char chunk[16384];
        const int read = stream.read(chunk, sizeof(chunk));
        if (read <= 0)
            return false;
        buffer.append(chunk, (size_t) read);
        return true;
    }

    bool isBlank(char c) {
        return c == ' ' || c == '\t';
    }

    int hexDigit(char c) {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    }

    // the hex size of a chunk line; blanks around it and extensions after ';' are ignored
    bool parseChunkSize(const std::string &line, size_t &size) {
        size_t position = 0;
        while (position < line.size() && isBlank(line[position]))
            ++position;
        const size_t start = position;
        uint64_t value = 0;
        for (; position < line.size() && hexDigit(line[position]) >= 0; ++position) {
            // 16 digits fill 64 bits
            if (position - start == 16)
                return false;
            value = (value << 4) | (uint64_t) hexDigit(line[position]);
        }
        if (position == start)
            return false;
        while (position < line.size() && isBlank(line[position]))
            ++position;
        if (position < line.size() && line[position] != ';')
            return false;
        if ((uint64_t) (size_t) value != value)
            return false;
        size = (size_t) value;
        return true;
    }

    bool parseLength(const std::string &text, size_t &length) {
        // 19 digits always fit 64 bits
        if (text.empty() || text.size() > 19)
            return false;
        uint64_t value = 0;
        for (const char c: text) {
            if (c < '0' || c > '9')
                return false;
            value = value * 10 + (uint64_t) (c - '0');
        }
        if ((uint64_t) (size_t) value != value)
            return false;
        length = (size_t) value;
        return true;
    }

    bool readChunked(NetStream &stream, std::string &pending, std::string &body, size_t limit) {
        for (;;) {
            size_t lineEnd;
            while ((lineEnd = pending.find("\r\n")) == std::string::npos) {
                if (pending.size() > MAX_HEAD_SIZE || !fill(stream, pending))
                    return false;
            }
            size_t size = 0;
            if (!parseChunkSize(pending.substr(0, lineEnd), size))
                return false;
            pending.erase(0, lineEnd + 2);
            if (size == 0) {
                // trailer fields end with an empty line
                size_t trailers = 0;
                for (;;) {
                    while ((lineEnd = pending.find("\r\n")) == std::string::npos) {
                        if (pending.size() > MAX_HEAD_SIZE || !fill(stream, pending))
                            return false;
                    }
                    trailers += lineEnd + 2;
                    if (trailers > MAX_HEAD_SIZE)
                        return false;
                    const bool last = lineEnd == 0;
                    pending.erase(0, lineEnd + 2);
                    if (last)
                        return true;
                }
            }
            if (limit != 0 && (body.size() > limit || size > limit - body.size()))
                return false;
            while (pending.size() < size || pending.size() - size < 2) {
                if (!fill(stream, pending))
                    return false;
            }
            if (pending.compare(size, 2, "\r\n") != 0)
                return false;
            body.append(pending, 0, size);
            pending.erase(0, size + 2);
        }
    }

    bool isTokenChar(char c) {
        return std::isalnum((unsigned char) c) || (c != '\0' && std::strchr("!#$%&'*+-.^_`|~", c) != nullptr);
    }

    bool isToken(const std::string &text) {
        return !text.empty() && std::all_of(text.begin(), text.end(), isTokenChar);
    }
}

bool HttpsClient::isRequestText(const std::string &text) {
    return std::none_of(text.begin(), text.end(), [](char c) { return (unsigned char) c <= ' ' || c == 0x7F; });
}

bool HttpsClient::isRequestHeaders(const Headers &headers) {
    for (const auto &header: headers) {
        if (!isToken(header.first))
            return false;
        for (const char c: header.second) {
            if (((unsigned char) c < ' ' && c != '\t') || c == 0x7F)
                return false;
        }
    }
    return true;
}

std::string HttpsClient::hostField(const NetUri &uri) {
    std::string host;
    std::string port;
    uri.hostAndPort(host, port);
    if (host.find(':') != std::string::npos)
        host = "[" + host + "]";
    return port.empty() ? host : host + ":" + port;
}

bool HttpsClient::readHead(NetStream &stream, std::string &head, std::string &rest, size_t limit) {
    std::string buffer = rest;
    rest.clear();
    for (;;) {
        const size_t end = buffer.find("\r\n\r\n");
        if (end != std::string::npos) {
            head = buffer.substr(0, end);
            rest = buffer.substr(end + 4);
            return true;
        }
        if (buffer.size() > limit || !fill(stream, buffer))
            return false;
    }
}

bool HttpsClient::parseHead(const std::string &head, int &status, Headers &headers) {
    const size_t lineEnd = head.find("\r\n");
    const std::string statusLine = head.substr(0, lineEnd);
    const size_t space = statusLine.find(' ');
    if (statusLine.compare(0, 5, "HTTP/") != 0 || space == std::string::npos)
        return false;
    status = std::atoi(statusLine.c_str() + space + 1);

    size_t position = lineEnd == std::string::npos ? head.size() : lineEnd + 2;
    while (position < head.size()) {
        size_t next = head.find("\r\n", position);
        if (next == std::string::npos)
            next = head.size();
        const std::string line = head.substr(position, next - position);
        const size_t colon = line.find(':');
        if (colon != std::string::npos)
            headers.emplace_back(trim(line.substr(0, colon)), trim(line.substr(colon + 1)));
        position = next + 2;
    }
    return true;
}

HttpsClient::Response HttpsClient::perform(const Request &request, NetStream &stream) {
    Response response;
    const NetUri uri = NetUri::split(request.mUrl);
    const std::string scheme = lower(uri.mScheme);
    const bool secure = scheme == "https";
    std::string host;
    std::string port;
    uri.hostAndPort(host, port);
    const std::string target = uri.target();
    const std::string hostValue = hostField(uri);
    if ((!secure && scheme != "http") || host.empty() || !isRequestText(target) || !isRequestText(hostValue)) {
        response.mErrorCode = -1;
        response.mErrorMessage = "invalid url";
        return response;
    }
    if (!isToken(request.mMethod) || !isRequestHeaders(request.mHeaders)) {
        response.mErrorCode = -1;
        response.mErrorMessage = "invalid header";
        return response;
    }
    if (port.empty())
        port = secure ? "443" : "80";

    if (!stream.open(host, port, secure, request.mTimeoutSeconds)) {
        response.mErrorCode = stream.getErrorCode();
        response.mErrorMessage = stream.getErrorMessage();
        return response;
    }

    std::string message = request.mMethod + " " + target + " HTTP/1.1\r\n";
    // the host always comes from the url: a caller's Host field could aim the request at another site
    message += "Host: " + hostValue + "\r\n";
    for (const auto &header: request.mHeaders) {
        if (lower(header.first) != "host")
            message += header.first + ": " + header.second + "\r\n";
    }
    if (!request.mBody.empty() || (request.mMethod != "GET" && request.mMethod != "HEAD"))
        message += "Content-Length: " + std::to_string(request.mBody.size()) + "\r\n";
    message += "Connection: close\r\n\r\n";
    message += request.mBody;

    std::string head;
    std::string rest;
    if (!stream.writeAll(message.data(), message.size()) || !readHead(stream, head, rest, MAX_HEAD_SIZE)
        || !parseHead(head, response.mStatus, response.mHeaders)) {
        response.mErrorCode = stream.getErrorCode() != 0 ? stream.getErrorCode() : -1;
        response.mErrorMessage = !stream.getErrorMessage().empty() ? stream.getErrorMessage()
                                                                   : std::string("malformed response");
        response.mHeaders.clear();
        response.mStatus = 0;
        return response;
    }

    const bool noBody = request.mMethod == "HEAD" || response.mStatus == 204 || response.mStatus == 304
                        || (response.mStatus >= 100 && response.mStatus < 200);
    const std::string *encoding = findHeader(response.mHeaders, "transfer-encoding");
    const std::string *length = findHeader(response.mHeaders, "content-length");
    bool complete = true;
    if (noBody) {
    } else if (encoding != nullptr && lower(*encoding).find("chunked") != std::string::npos) {
        complete = readChunked(stream, rest, response.mBody, request.mMaxResponseBytes);
    } else if (length != nullptr) {
        size_t wanted = 0;
        if (!parseLength(*length, wanted)
            || (request.mMaxResponseBytes != 0 && wanted > request.mMaxResponseBytes)) {
            complete = false;
        } else {
            response.mBody = rest;
            while (response.mBody.size() < wanted && fill(stream, response.mBody)) {
            }
            complete = response.mBody.size() >= wanted;
            response.mBody.resize(std::min(response.mBody.size(), wanted));
        }
    } else {
        response.mBody = rest;
        while (fill(stream, response.mBody)) {
            if (request.mMaxResponseBytes != 0 && response.mBody.size() > request.mMaxResponseBytes) {
                complete = false;
                break;
            }
        }
        if (stream.getErrorCode() != 0 || !stream.getErrorMessage().empty())
            complete = false;
    }

    if (!complete) {
        response.mErrorCode = stream.getErrorCode() != 0 ? stream.getErrorCode() : -1;
        response.mErrorMessage = !stream.getErrorMessage().empty() ? stream.getErrorMessage()
                                                                   : std::string("incomplete response");
        response.mHeaders.clear();
        response.mBody.clear();
        response.mStatus = 0;
        return response;
    }
    response.mCompleted = true;
    return response;
}

bool HttpsClient::get(const std::string &url, std::string &body, int timeoutSeconds) {
    if (NetUri::split(url).mScheme != "https") {
        LOG_WARN(LogAreaID::Network, "Cannot fetch %s because the url is malformed", url.c_str());
        return false;
    }

    Request request;
    request.mUrl = url;
    request.mHeaders = {{"User-Agent", "Falcon"}, {"Accept", "application/json"}};
    request.mTimeoutSeconds = (uint32_t) (timeoutSeconds < 0 ? 0 : timeoutSeconds);
    request.mMaxResponseBytes = MAX_GET_RESPONSE_SIZE;

    NetStream stream;
    const Response response = perform(request, stream);
    if (!response.mCompleted) {
        LOG_WARN(LogAreaID::Network, "Failed to fetch %s: %s", url.c_str(), response.mErrorMessage.c_str());
        return false;
    }
    if (response.mStatus != 200) {
        LOG_WARN(LogAreaID::Network, "Unexpected http status %d while fetching %s", response.mStatus, url.c_str());
        return false;
    }
    body = response.mBody;
    return true;
}
