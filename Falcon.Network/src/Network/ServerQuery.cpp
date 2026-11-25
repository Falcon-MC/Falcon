#include "Network/ServerQuery.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <random>
#include <vector>

namespace {

#ifdef _WIN32
    using SocketHandle = SOCKET;
    const SocketHandle INVALID_HANDLE = INVALID_SOCKET;
#else
    using SocketHandle = int;
    const SocketHandle INVALID_HANDLE = -1;
#endif

    const unsigned char UNCONNECTED_PING = 0x01;
    const unsigned char UNCONNECTED_PONG = 0x1c;
    const std::array<unsigned char, 16> OFFLINE_MESSAGE_ID = {0x00, 0xff, 0xff, 0x00, 0xfe, 0xfe, 0xfe, 0xfe,
                                                              0xfd, 0xfd, 0xfd, 0xfd, 0x12, 0x34, 0x56, 0x78};
    const size_t PONG_HEADER_SIZE = 1 + 8 + 8 + 16 + 2;
    const size_t MAX_DATAGRAM_SIZE = 2048;

    void initializeSockets() {
#ifdef _WIN32
        static std::once_flag initialized;
        std::call_once(initialized, [] {
            WSADATA data;
            WSAStartup(MAKEWORD(2, 2), &data);
        });
#endif
    }

    void closeHandle(SocketHandle handle) {
#ifdef _WIN32
        closesocket(handle);
#else
        close(handle);
#endif
    }

    bool waitReadable(SocketHandle handle, int timeoutMs) {
#ifdef _WIN32
        WSAPOLLFD descriptor{};
        descriptor.fd = handle;
        descriptor.events = POLLRDNORM;
        return WSAPoll(&descriptor, 1, timeoutMs) > 0;
#else
        pollfd descriptor{};
        descriptor.fd = handle;
        descriptor.events = POLLIN;
        return poll(&descriptor, 1, timeoutMs) > 0;
#endif
    }

    void appendLong(std::string &buffer, uint64_t value) {
        for (int shift = 56; shift >= 0; shift -= 8)
            buffer.push_back((char) ((value >> shift) & 0xff));
    }

    std::vector<std::string> split(const std::string &value, char separator) {
        std::vector<std::string> parts;
        size_t start = 0;
        for (;;) {
            const size_t end = value.find(separator, start);
            parts.push_back(value.substr(start, end == std::string::npos ? std::string::npos : end - start));
            if (end == std::string::npos)
                return parts;
            start = end + 1;
        }
    }

    std::string part(const std::vector<std::string> &parts, size_t index) {
        return index < parts.size() ? parts[index] : std::string();
    }

}

void ServerQuery::readAnnouncement(const std::string &announcement, ServerQueryResult &outResult) {
    const std::vector<std::string> parts = split(announcement, ';');
    outResult.mMotd = part(parts, 1);
    outResult.mProtocol = std::atoi(part(parts, 2).c_str());
    outResult.mVersion = part(parts, 3);
    outResult.mPlayerCount = std::atoi(part(parts, 4).c_str());
    outResult.mMaxPlayers = std::atoi(part(parts, 5).c_str());
    outResult.mSubMotd = part(parts, 7);
}

bool ServerQuery::query(const std::string &host, unsigned short port, ServerQueryResult &outResult,
                        std::string &outError, unsigned int timeoutMs) {
    initializeSockets();

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo *addresses = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &addresses) != 0 || addresses == nullptr) {
        outError = "could not resolve " + host;
        return false;
    }

    const SocketHandle handle = socket(addresses->ai_family, SOCK_DGRAM, 0);
    if (handle == INVALID_HANDLE) {
        freeaddrinfo(addresses);
        outError = "could not create a socket";
        return false;
    }

    std::mt19937_64 random(std::random_device{}());
    std::string ping(1, (char) UNCONNECTED_PING);
    appendLong(ping, (uint64_t) std::chrono::steady_clock::now().time_since_epoch().count());
    ping.append((const char *) OFFLINE_MESSAGE_ID.data(), OFFLINE_MESSAGE_ID.size());
    appendLong(ping, random());

    const auto start = std::chrono::steady_clock::now();
    const bool sent = sendto(handle, ping.data(), (int) ping.size(), 0, addresses->ai_addr,
                             (int) addresses->ai_addrlen) >= 0;
    freeaddrinfo(addresses);
    if (!sent) {
        closeHandle(handle);
        outError = "could not send the ping";
        return false;
    }

    const auto deadline = start + std::chrono::milliseconds(timeoutMs);
    char buffer[MAX_DATAGRAM_SIZE];
    for (;;) {
        const auto remaining =
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        if (remaining.count() <= 0 || !waitReadable(handle, (int) remaining.count())) {
            closeHandle(handle);
            outError = "the server did not answer";
            return false;
        }

        const auto received = recv(handle, buffer, sizeof(buffer), 0);
        if (received < (int) PONG_HEADER_SIZE || (unsigned char) buffer[0] != UNCONNECTED_PONG ||
            std::memcmp(buffer + 17, OFFLINE_MESSAGE_ID.data(), OFFLINE_MESSAGE_ID.size()) != 0)
            continue;

        const size_t length = (size_t) ((unsigned char) buffer[33] << 8 | (unsigned char) buffer[34]);
        if (PONG_HEADER_SIZE + length > (size_t) received)
            continue;

        closeHandle(handle);
        outResult = ServerQueryResult();
        outResult.mLatencyMs =
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        readAnnouncement(std::string(buffer + PONG_HEADER_SIZE, length), outResult);
        return true;
    }
}
