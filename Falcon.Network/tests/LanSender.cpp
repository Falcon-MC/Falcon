#include "Network/NetherNet/NetherNetDiscovery.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    bool local4(const char *address) {
        struct sockaddr_in from;
        std::memset(&from, 0, sizeof(from));
        from.sin_family = AF_INET;
        inet_pton(AF_INET, address, &from.sin_addr);
        return nethernet::DiscoveryListener::isLocalAddress(&from);
    }

    bool local6(const char *address) {
        struct sockaddr_in6 from;
        std::memset(&from, 0, sizeof(from));
        from.sin6_family = AF_INET6;
        inet_pton(AF_INET6, address, &from.sin6_addr);
        return nethernet::DiscoveryListener::isLocalAddress(&from);
    }
}

int main() {
    try {
        require(local4("127.0.0.1"), "loopback is local");
        require(local4("10.1.2.3"), "10/8 is local");
        require(local4("172.16.0.1") && local4("172.31.255.255"), "172.16/12 is local");
        require(!local4("172.32.0.1") && !local4("172.15.255.255"), "outside 172.16/12 is not local");
        require(local4("192.168.1.115"), "192.168/16 is local");
        require(local4("169.254.10.10"), "link local is local");
        require(!local4("8.8.8.8") && !local4("193.168.1.1"), "a public address is not local");
        require(local6("::1"), "ipv6 loopback is local");
        require(local6("fd12::1") && local6("fe80::1"), "unique and link local ipv6 are local");
        require(!local6("2001:db8::1"), "a global ipv6 address is not local");
    } catch (const std::exception &error) {
        std::cerr << "LAN sender test failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "LAN sender checks passed\n";
    return 0;
}
