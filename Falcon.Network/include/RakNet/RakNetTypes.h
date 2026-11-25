#pragma once

#include "RakNet/RakNetDefines.h"
#include "RakNet/GetTime.h"

#include <cstdint>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else

#include <netinet/in.h>
#include <sys/socket.h>

#endif

namespace RakNet {

    class BitStream;

    struct uint24_t {
        uint32_t val;

        uint24_t() : val(0) {}

        uint24_t(uint32_t v) : val(v & 0x00ffffff) {}

        operator uint32_t() const { return val; }

        uint24_t &operator++() {
            val = (val + 1) & 0x00ffffff;
            return *this;
        }

        uint24_t operator++(int) {
            uint24_t tmp = *this;
            ++(*this);
            return tmp;
        }
    };

    struct SystemAddress {
        union {
            sockaddr_in addr4;
            sockaddr_in6 addr6;
        };

        SystemIndex systemIndex;

        SystemAddress();

        SystemAddress(const char *address, unsigned short port);

        bool FromString(const char *address, unsigned short port);

        std::string ToString(bool writePort = true) const;

        unsigned short GetPort() const;

        void SetPort(unsigned short port);

        unsigned short GetIPVersion() const;

        bool IsLoopback() const;

        bool IsUnassigned() const;

        void Serialize(BitStream *out) const;

        bool Deserialize(BitStream *in);

        bool operator==(const SystemAddress &right) const;

        bool operator!=(const SystemAddress &right) const;

        bool operator<(const SystemAddress &right) const;
    };

    struct SystemAddressHash {
        size_t operator()(const SystemAddress &address) const;
    };

    struct RakNetGUID {
        uint64_t g;
        SystemIndex systemIndex;

        RakNetGUID() : g((uint64_t) -1), systemIndex((SystemIndex) -1) {}

        explicit RakNetGUID(uint64_t guid) : g(guid), systemIndex((SystemIndex) -1) {}

        std::string ToString() const;

        bool operator==(const RakNetGUID &right) const { return g == right.g; }

        bool operator!=(const RakNetGUID &right) const { return g != right.g; }

        bool operator<(const RakNetGUID &right) const { return g < right.g; }
    };

    extern const SystemAddress UNASSIGNED_SYSTEM_ADDRESS;
    extern const RakNetGUID UNASSIGNED_RAKNET_GUID;

    struct AddressOrGUID {
        RakNetGUID rakNetGuid;
        SystemAddress systemAddress;

        AddressOrGUID() = default;

        AddressOrGUID(const SystemAddress &address) : systemAddress(address) {}

        AddressOrGUID(const RakNetGUID &guid) : rakNetGuid(guid) {}

        bool IsUndefined() const {
            return rakNetGuid == UNASSIGNED_RAKNET_GUID && systemAddress == UNASSIGNED_SYSTEM_ADDRESS;
        }
    };

    enum ConnectionState {
        IS_PENDING,
        IS_CONNECTING,
        IS_CONNECTED,
        IS_DISCONNECTING,
        IS_SILENTLY_DISCONNECTING,
        IS_DISCONNECTED,
        IS_NOT_CONNECTED
    };

    enum StartupResult {
        RAKNET_STARTED,
        RAKNET_ALREADY_STARTED,
        INVALID_SOCKET_DESCRIPTORS,
        INVALID_MAX_CONNECTIONS,
        SOCKET_FAMILY_NOT_SUPPORTED,
        SOCKET_PORT_ALREADY_IN_USE,
        SOCKET_FAILED_TO_BIND,
        SOCKET_FAILED_TEST_SEND,
        FAILED_TO_CREATE_NETWORK_THREAD
    };

    enum ConnectionAttemptResult {
        CONNECTION_ATTEMPT_STARTED,
        INVALID_PARAMETER,
        CANNOT_RESOLVE_DOMAIN_NAME,
        ALREADY_CONNECTED_TO_ENDPOINT,
        CONNECTION_ATTEMPT_ALREADY_IN_PROGRESS,
        SECURITY_INITIALIZATION_FAILED
    };

    struct Packet {
        SystemAddress systemAddress;
        RakNetGUID guid;
        unsigned int length;
        BitSize_t bitSize;
        unsigned char *data;
        bool deleteData;
        bool wasGeneratedLocally;
    };

    struct SocketDescriptor {
        unsigned short port;
        char hostAddress[64];
        short socketFamily;

        SocketDescriptor() : port(0), socketFamily(AF_INET) { hostAddress[0] = 0; }

        SocketDescriptor(unsigned short _port, const char *_hostAddress)
                : port(_port), socketFamily(AF_INET) {
            hostAddress[0] = 0;
            if (_hostAddress) {
                strncpy(hostAddress, _hostAddress, sizeof(hostAddress) - 1);
                hostAddress[sizeof(hostAddress) - 1] = 0;
            }
        }
    };

}
