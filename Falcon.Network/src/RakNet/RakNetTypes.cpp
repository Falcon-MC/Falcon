#include "RakNet/RakNetTypes.h"
#include "RakNet/BitStream.h"

#include <cstdio>

#ifndef _WIN32

#include <arpa/inet.h>

#endif

namespace {

    const unsigned short WIRE_FAMILY_INET6 = 23;

}

namespace RakNet {

    const SystemAddress UNASSIGNED_SYSTEM_ADDRESS;
    const RakNetGUID UNASSIGNED_RAKNET_GUID;

    SystemAddress::SystemAddress() : systemIndex((SystemIndex) -1) {
        memset(&addr6, 0, sizeof(addr6));
        addr4.sin_family = AF_INET;
    }

    SystemAddress::SystemAddress(const char *address, unsigned short port) : systemIndex((SystemIndex) -1) {
        memset(&addr6, 0, sizeof(addr6));
        addr4.sin_family = AF_INET;
        FromString(address, port);
    }

    bool SystemAddress::FromString(const char *address, unsigned short port) {
        memset(&addr6, 0, sizeof(addr6));

        if (address && strchr(address, ':')) {
            addr6.sin6_family = AF_INET6;
            addr6.sin6_port = htons(port);
            return inet_pton(AF_INET6, address, &addr6.sin6_addr) == 1;
        }

        addr4.sin_family = AF_INET;
        addr4.sin_port = htons(port);

        if (!address || address[0] == 0) {
            addr4.sin_addr.s_addr = INADDR_ANY;
            return true;
        }

        return inet_pton(AF_INET, address, &addr4.sin_addr) == 1;
    }

    std::string SystemAddress::ToString(bool writePort) const {
        char buffer[INET6_ADDRSTRLEN] = {0};

        if (GetIPVersion() == 6) {
            inet_ntop(AF_INET6, (void *) &addr6.sin6_addr, buffer, sizeof(buffer));
            if (!writePort)
                return std::string(buffer);

            char full[INET6_ADDRSTRLEN + 16];
            snprintf(full, sizeof(full), "[%s]:%u", buffer, GetPort());
            return std::string(full);
        }

        inet_ntop(AF_INET, (void *) &addr4.sin_addr, buffer, sizeof(buffer));
        if (!writePort)
            return std::string(buffer);

        char full[INET6_ADDRSTRLEN + 16];
        snprintf(full, sizeof(full), "%s:%u", buffer, GetPort());
        return std::string(full);
    }

    unsigned short SystemAddress::GetPort() const {
        return ntohs(addr4.sin_port);
    }

    void SystemAddress::SetPort(unsigned short port) {
        addr4.sin_port = htons(port);
    }

    unsigned short SystemAddress::GetIPVersion() const {
        return addr4.sin_family == AF_INET6 ? 6 : 4;
    }

    bool SystemAddress::IsLoopback() const {
        if (GetIPVersion() == 4)
            return (ntohl(addr4.sin_addr.s_addr) >> 24) == 127;
        return IN6_IS_ADDR_LOOPBACK(&addr6.sin6_addr) != 0;
    }

    bool SystemAddress::IsUnassigned() const {
        return *this == UNASSIGNED_SYSTEM_ADDRESS;
    }

    void SystemAddress::Serialize(BitStream *out) const {
        if (GetIPVersion() == 6) {
            const unsigned char *bytes = (const unsigned char *) &addr6.sin6_addr;
            bool mapped = bytes[10] == 0xff && bytes[11] == 0xff;
            for (int i = 0; i < 10 && mapped; i++)
                mapped = bytes[i] == 0;

            if (mapped) {
                out->Write((unsigned char) 4);
                for (int i = 12; i < 16; i++)
                    out->Write((unsigned char) (~bytes[i]));
                out->Write(GetPort());
                return;
            }

            out->Write((unsigned char) 6);
            out->Write((unsigned char) (WIRE_FAMILY_INET6 & 0xff));
            out->Write((unsigned char) ((WIRE_FAMILY_INET6 >> 8) & 0xff));
            out->Write(GetPort());
            out->Write((uint32_t) addr6.sin6_flowinfo);
            out->Write((const char *) &addr6.sin6_addr, 16);
            out->Write((uint32_t) addr6.sin6_scope_id);
            return;
        }

        out->Write((unsigned char) 4);

        const unsigned char *octets = (const unsigned char *) &addr4.sin_addr.s_addr;
        for (int i = 0; i < 4; i++)
            out->Write((unsigned char) (~octets[i]));

        out->Write(GetPort());
    }

    bool SystemAddress::Deserialize(BitStream *in) {
        unsigned char version;
        if (!in->Read(version))
            return false;

        if (version == 6) {
            unsigned char familyLow, familyHigh;
            uint16_t port;
            uint32_t flowInfo, scopeId;

            if (!in->Read(familyLow) || !in->Read(familyHigh) || !in->Read(port) || !in->Read(flowInfo))
                return false;

            memset(&addr6, 0, sizeof(addr6));
            addr6.sin6_family = AF_INET6;
            addr6.sin6_port = htons(port);
            addr6.sin6_flowinfo = flowInfo;

            if (!in->Read((char *) &addr6.sin6_addr, 16) || !in->Read(scopeId))
                return false;

            addr6.sin6_scope_id = scopeId;
            return true;
        }

        if (version != 4)
            return false;

        unsigned char octets[4];
        for (int i = 0; i < 4; i++) {
            unsigned char octet;
            if (!in->Read(octet))
                return false;
            octets[i] = (unsigned char) (~octet);
        }

        uint16_t port;
        if (!in->Read(port))
            return false;

        memset(&addr6, 0, sizeof(addr6));
        addr4.sin_family = AF_INET;
        memcpy(&addr4.sin_addr.s_addr, octets, 4);
        addr4.sin_port = htons(port);
        return true;
    }

    bool SystemAddress::operator==(const SystemAddress &right) const {
        if (addr4.sin_family != right.addr4.sin_family)
            return false;

        if (GetIPVersion() == 6)
            return addr6.sin6_port == right.addr6.sin6_port &&
                   memcmp(&addr6.sin6_addr, &right.addr6.sin6_addr, sizeof(addr6.sin6_addr)) == 0;

        return addr4.sin_port == right.addr4.sin_port && addr4.sin_addr.s_addr == right.addr4.sin_addr.s_addr;
    }

    bool SystemAddress::operator!=(const SystemAddress &right) const {
        return !(*this == right);
    }

    bool SystemAddress::operator<(const SystemAddress &right) const {
        if (GetIPVersion() != right.GetIPVersion())
            return GetIPVersion() < right.GetIPVersion();

        if (GetIPVersion() == 6) {
            const int cmp = memcmp(&addr6.sin6_addr, &right.addr6.sin6_addr, sizeof(addr6.sin6_addr));
            if (cmp != 0)
                return cmp < 0;
            return addr6.sin6_port < right.addr6.sin6_port;
        }

        if (addr4.sin_addr.s_addr != right.addr4.sin_addr.s_addr)
            return addr4.sin_addr.s_addr < right.addr4.sin_addr.s_addr;
        return addr4.sin_port < right.addr4.sin_port;
    }

    size_t SystemAddressHash::operator()(const SystemAddress &address) const {
        size_t hash = 1469598103934665603ull;

        const unsigned char *bytes;
        size_t length;

        if (address.GetIPVersion() == 6) {
            bytes = (const unsigned char *) &address.addr6.sin6_addr;
            length = sizeof(address.addr6.sin6_addr);
        } else {
            bytes = (const unsigned char *) &address.addr4.sin_addr;
            length = sizeof(address.addr4.sin_addr);
        }

        for (size_t i = 0; i < length; i++) {
            hash ^= bytes[i];
            hash *= 1099511628211ull;
        }

        hash ^= address.addr4.sin_port;
        hash *= 1099511628211ull;
        return hash;
    }

    std::string RakNetGUID::ToString() const {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long) g);
        return std::string(buffer);
    }

}
