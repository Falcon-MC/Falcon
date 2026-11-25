#include "RakNet/RakNetSocket2.h"

#include <cerrno>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define CLOSE_SOCKET ::closesocket
typedef int socklen_t;

namespace {

    struct WinsockInitializer {
        WinsockInitializer() {
            WSADATA data;
            WSAStartup(MAKEWORD(2, 2), &data);
        }

        ~WinsockInitializer() {
            WSACleanup();
        }
    };

    WinsockInitializer gWinsockInitializer;

}
#else

#include <poll.h>
#include <unistd.h>

#define CLOSE_SOCKET ::close
#endif

namespace RakNet {

    RakNetSocket2::RakNetSocket2() : sock(-1) {}

    RakNetSocket2::~RakNetSocket2() {
        Close();
    }

    bool RakNetSocket2::IsOpen() const {
        return sock != -1;
    }

    RNS2BindResult RakNetSocket2::Bind(const char *hostAddress, unsigned short port, short socketFamily) {
        Close();

        sock = (int) socket(socketFamily, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == -1)
            return BR_FAILED_TO_BIND_SOCKET;

        int reuse = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *) &reuse, sizeof(reuse));

        if (socketFamily == AF_INET6) {
            int v6Only = 0;
            setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char *) &v6Only, sizeof(v6Only));
        }

        int bufferSize = 1024 * 1024;
        setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char *) &bufferSize, sizeof(bufferSize));
        setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char *) &bufferSize, sizeof(bufferSize));

        SystemAddress address;
        if (!address.FromString(hostAddress, port)) {
            Close();
            return BR_FAILED_TO_BIND_SOCKET;
        }

        const socklen_t addressLength = socketFamily == AF_INET6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in);
        if (bind(sock, (const sockaddr *) &address.addr4, addressLength) != 0) {
            Close();
            return BR_FAILED_TO_BIND_SOCKET;
        }

        sockaddr_in6 bound;
        socklen_t boundLength = sizeof(bound);
        if (getsockname(sock, (sockaddr *) &bound, &boundLength) == 0)
            memcpy(&boundAddress.addr6, &bound, boundLength);
        else
            boundAddress = address;

        return BR_SUCCESS;
    }

    void RakNetSocket2::Close() {
        if (sock != -1) {
            CLOSE_SOCKET(sock);
            sock = -1;
        }
    }

    int RakNetSocket2::Send(const char *data, int length, const SystemAddress &systemAddress) {
        if (sock == -1)
            return -1;

        const socklen_t addressLength =
                systemAddress.GetIPVersion() == 6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in);

        return (int) sendto(sock, data, length, 0, (const sockaddr *) &systemAddress.addr4, addressLength);
    }

    bool RakNetSocket2::RecvFrom(RNS2RecvStruct *recvStruct, int timeoutMs) {
        if (sock == -1)
            return false;

#ifdef _WIN32
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(sock, &readSet);

        timeval timeout;
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;

        if (select(sock + 1, &readSet, nullptr, nullptr, &timeout) <= 0)
            return false;
#else
        pollfd pollDescriptor;
        pollDescriptor.fd = sock;
        pollDescriptor.events = POLLIN;
        pollDescriptor.revents = 0;

        if (poll(&pollDescriptor, 1, timeoutMs) <= 0)
            return false;
#endif

        sockaddr_in6 from;
        socklen_t fromLength = sizeof(from);

        const int bytesRead = (int) recvfrom(sock, recvStruct->data, sizeof(recvStruct->data), 0,
                                             (sockaddr *) &from, &fromLength);
        if (bytesRead <= 0)
            return false;

        recvStruct->bytesRead = bytesRead;
        memset(&recvStruct->systemAddress.addr6, 0, sizeof(recvStruct->systemAddress.addr6));
        memcpy(&recvStruct->systemAddress.addr6, &from, fromLength);
        recvStruct->timeRead = GetTimeUS();
        return true;
    }

}
