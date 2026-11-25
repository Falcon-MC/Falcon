#include "RakNet/RakPeer.h"
#include "RakNet/MessageIdentifiers.h"

#include <chrono>
#include <cstring>

#include <openssl/evp.h>

#include <cstdlib>
#include <random>
#ifndef _WIN32
#include <ifaddrs.h>
#include <net/if.h>
#else
#include <ws2tcpip.h>
#endif

namespace RakNet {

    static std::vector<SystemAddress> LocalAddresses(const SystemAddress &boundAddress) {
        std::vector<SystemAddress> addresses;
        const auto add = [&](const sockaddr *source) {
            if (source == nullptr || (source->sa_family != AF_INET && source->sa_family != AF_INET6))
                return;
            if (source->sa_family == AF_INET6 && boundAddress.GetIPVersion() != 6)
                return;
            SystemAddress address;
            memcpy(&address.addr4, source, source->sa_family == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6));
            if ((source->sa_family == AF_INET && address.addr4.sin_addr.s_addr == INADDR_ANY)
                || (source->sa_family == AF_INET6 && IN6_IS_ADDR_UNSPECIFIED(&address.addr6.sin6_addr)))
                return;
            address.SetPort(boundAddress.GetPort());
            if (address.IsUnassigned() || (address.GetIPVersion() == 4 && address.IsLoopback()))
                return;
            for (const auto &existing : addresses)
                if (existing == address)
                    return;
            if (addresses.size() < MAXIMUM_NUMBER_OF_INTERNAL_IDS)
                addresses.push_back(address);
        };
#ifndef _WIN32
        ifaddrs *interfaces = nullptr;
        if (getifaddrs(&interfaces) == 0) {
            for (auto entry = interfaces; entry != nullptr; entry = entry->ifa_next)
                if (entry->ifa_flags & IFF_UP)
                    add(entry->ifa_addr);
            freeifaddrs(interfaces);
        }
#else
        char hostname[256] = {};
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            addrinfo hints = {};
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_DGRAM;
            addrinfo *result = nullptr;
            if (getaddrinfo(hostname, nullptr, &hints, &result) == 0) {
                for (auto entry = result; entry != nullptr; entry = entry->ai_next)
                    add(entry->ai_addr);
                freeaddrinfo(result);
            }
        }
#endif
        return addresses;
    }

    static const int CONNECTION_MTU_SIZES[] = {MAXIMUM_OUTGOING_MTU_SIZE, 1200, MINIMUM_OUTGOING_MTU_SIZE};
    static const size_t CONNECTION_MTU_SIZE_COUNT = sizeof(CONNECTION_MTU_SIZES) / sizeof(CONNECTION_MTU_SIZES[0]);

    /**
     * How much the network loop reads before it acknowledges, resends and
     * sends again, so a steady stream from the server cannot starve the acks
     * and pings it waits for.
     */
    static const int MAX_DATAGRAMS_PER_UPDATE = 256;
    static const TimeMS MAX_RECEIVE_MS_PER_UPDATE = 10;

    static uint64_t GenerateGUID() {
        std::random_device randomDevice;
        std::mt19937_64 generator(((uint64_t) randomDevice() << 32) ^ GetTimeUS());
        return generator() | 0x8000000000000000ULL;
    }

    RakPeerInterface *RakPeerInterface::GetInstance() {
        return new RakPeer();
    }

    void RakPeerInterface::DestroyInstance(RakPeerInterface *instance) {
        delete instance;
    }

    namespace {
        // a slot is held on one small datagram: few per address, and not for long
        const unsigned int MAX_HALF_OPEN_PER_ADDRESS = 8;
        const TimeMS HALF_OPEN_TIMEOUT_MS = 5000;
    }

    RakPeer::RakPeer()
            : endThreads(true), myGuid(GenerateGUID()), maximumNumberOfPeers(0),
              maximumIncomingConnections(0), nextSystemIndex(0) {
        std::random_device device;
        for (unsigned char &byte: cookieSecret)
            byte = (unsigned char) device();
    }

    uint32_t RakPeer::HandshakeCookie(const SystemAddress &systemAddress) const {
        const std::string address = systemAddress.ToString(true);

        unsigned char digest[EVP_MAX_MD_SIZE];
        unsigned int length = 0;
        EVP_MD_CTX *context = EVP_MD_CTX_new();
        uint32_t cookie = 0;
        if (context != nullptr
            && EVP_DigestInit_ex(context, EVP_sha256(), nullptr) == 1
            && EVP_DigestUpdate(context, cookieSecret, sizeof(cookieSecret)) == 1
            && EVP_DigestUpdate(context, address.data(), address.size()) == 1
            && EVP_DigestFinal_ex(context, digest, &length) == 1 && length >= sizeof(cookie))
            std::memcpy(&cookie, digest, sizeof(cookie));
        EVP_MD_CTX_free(context);
        return cookie;
    }

    namespace {
        // what counts as one host: an IPv4 address, or an IPv6 /64, since a single machine owns all of it.
        // a v4 address mapped into v6 (dual-stack socket) stays one IPv4 host
        std::string HostKey(const SystemAddress &systemAddress) {
            if (systemAddress.addr4.sin_family == AF_INET)
                return std::string((const char *) &systemAddress.addr4.sin_addr, 4);

            const unsigned char *bytes = (const unsigned char *) &systemAddress.addr6.sin6_addr;
            if (IN6_IS_ADDR_V4MAPPED(&systemAddress.addr6.sin6_addr))
                return std::string((const char *) bytes + 12, 4);
            return std::string((const char *) bytes, 8);
        }
    }

    bool RakPeer::HasHalfOpenRoomFor(const SystemAddress &systemAddress) const {
        // handshakes share the peer list with players: they may fill a quarter of it, never all
        const unsigned int budget = maximumNumberOfPeers / 4 > MAX_HALF_OPEN_PER_ADDRESS
                                    ? maximumNumberOfPeers / 4 : MAX_HALF_OPEN_PER_ADDRESS;
        const std::string host = HostKey(systemAddress);
        unsigned int fromHost = 0;
        unsigned int total = 0;
        for (auto &entry: remoteSystemList) {
            if (entry.second->connectMode != IS_CONNECTING || entry.second->weStartedTheConnection)
                continue;
            total++;
            if (HostKey(entry.second->systemAddress) == host)
                fromHost++;
        }
        return fromHost < MAX_HALF_OPEN_PER_ADDRESS && total < budget;
    }

    RakPeer::~RakPeer() {
        Shutdown(0);
    }

    StartupResult RakPeer::Startup(unsigned int maxConnections, SocketDescriptor *socketDescriptors,
                                   unsigned int socketDescriptorCount) {
        if (!endThreads)
            return RAKNET_ALREADY_STARTED;

        if (maxConnections == 0)
            return INVALID_MAX_CONNECTIONS;

        if (socketDescriptors == nullptr || socketDescriptorCount == 0)
            return INVALID_SOCKET_DESCRIPTORS;

        const RNS2BindResult bindResult = socket.Bind(socketDescriptors[0].hostAddress, socketDescriptors[0].port,
                                                      socketDescriptors[0].socketFamily);
        if (bindResult != BR_SUCCESS)
            return SOCKET_FAILED_TO_BIND;

        maximumNumberOfPeers = maxConnections;
        if (maximumIncomingConnections > maxConnections)
            maximumIncomingConnections = (unsigned short) maxConnections;

        endThreads = false;
        networkThread = std::thread(&RakPeer::UpdateNetworkLoop, this);

        return RAKNET_STARTED;
    }

    void RakPeer::Shutdown(unsigned int blockDuration) {
        if (endThreads)
            return;

        if (blockDuration > 0) {
            const TimeMS startTime = GetTimeMS();
            while (GetTimeMS() - startTime < blockDuration) {
                std::lock_guard<std::mutex> guard(remoteSystemMutex);
                bool dataWaiting = false;
                for (auto &entry: remoteSystemList) {
                    if (entry.second->reliabilityLayer.IsOutgoingDataWaiting()) {
                        dataWaiting = true;
                        break;
                    }
                }
                if (!dataWaiting)
                    break;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        endThreads = true;

        if (networkThread.joinable())
            networkThread.join();

        socket.Close();

        {
            std::lock_guard<std::mutex> guard(remoteSystemMutex);
            remoteSystemList.clear();
        }

        {
            std::lock_guard<std::mutex> guard(packetQueueMutex);
            for (Packet *packet: packetQueue)
                DeallocatePacket(packet);
            packetQueue.clear();
        }
    }

    void RakPeer::SetMaximumIncomingConnections(unsigned short numberAllowed) {
        maximumIncomingConnections = numberAllowed;
    }

    unsigned short RakPeer::NumberOfConnections() const {
        std::lock_guard<std::mutex> guard(remoteSystemMutex);
        return NumberOfConnectionsUnlocked();
    }

    unsigned short RakPeer::NumberOfConnectionsUnlocked() const {
        unsigned short count = 0;
        for (auto &entry: remoteSystemList) {
            if (entry.second->connectMode == IS_CONNECTED)
                count++;
        }
        return count;
    }

    void RakPeer::SetOfflinePingResponse(const char *data, unsigned int length) {
        std::lock_guard<std::mutex> guard(offlinePingResponseMutex);
        offlinePingResponse.assign((const unsigned char *) data, (const unsigned char *) data + length);
    }

    Packet *RakPeer::AllocPacket(unsigned int dataSize, const SystemAddress &systemAddress, const RakNetGUID &guid) {
        Packet *packet = new Packet();
        packet->data = new unsigned char[dataSize];
        packet->length = dataSize;
        packet->bitSize = BYTES_TO_BITS(dataSize);
        packet->systemAddress = systemAddress;
        packet->guid = guid;
        packet->deleteData = true;
        packet->wasGeneratedLocally = false;
        return packet;
    }

    void RakPeer::DeallocatePacket(Packet *packet) {
        if (!packet)
            return;

        if (packet->deleteData)
            delete[] packet->data;
        delete packet;
    }

    void RakPeer::PushBackPacket(Packet *packet) {
        std::lock_guard<std::mutex> guard(packetQueueMutex);
        packetQueue.push_back(packet);
    }

    Packet *RakPeer::Receive() {
        std::lock_guard<std::mutex> guard(packetQueueMutex);

        if (packetQueue.empty())
            return nullptr;

        Packet *packet = packetQueue.front();
        packetQueue.pop_front();
        return packet;
    }

    RakPeer::RemoteSystemStruct *RakPeer::GetRemoteSystemFromSystemAddress(const SystemAddress &systemAddress) const {
        auto it = remoteSystemList.find(systemAddress);
        return it == remoteSystemList.end() ? nullptr : it->second.get();
    }

    RakPeer::RemoteSystemStruct *RakPeer::GetRemoteSystemFromGUID(const RakNetGUID &guid) const {
        for (auto &entry: remoteSystemList) {
            if (entry.second->guid == guid)
                return entry.second.get();
        }
        return nullptr;
    }

    SystemAddress RakPeer::GetSystemAddressFromGuid(const RakNetGUID input) const {
        std::lock_guard<std::mutex> guard(remoteSystemMutex);

        RemoteSystemStruct *remoteSystem = GetRemoteSystemFromGUID(input);
        return remoteSystem ? remoteSystem->systemAddress : UNASSIGNED_SYSTEM_ADDRESS;
    }

    RakNetGUID RakPeer::GetGuidFromSystemAddress(const SystemAddress input) const {
        std::lock_guard<std::mutex> guard(remoteSystemMutex);

        RemoteSystemStruct *remoteSystem = GetRemoteSystemFromSystemAddress(input);
        return remoteSystem ? remoteSystem->guid : UNASSIGNED_RAKNET_GUID;
    }

    ConnectionState RakPeer::GetConnectionState(const AddressOrGUID systemIdentifier) {
        std::lock_guard<std::mutex> guard(remoteSystemMutex);

        RemoteSystemStruct *remoteSystem = systemIdentifier.rakNetGuid == UNASSIGNED_RAKNET_GUID
                                           ? GetRemoteSystemFromSystemAddress(systemIdentifier.systemAddress)
                                           : GetRemoteSystemFromGUID(systemIdentifier.rakNetGuid);

        return remoteSystem ? remoteSystem->connectMode : IS_NOT_CONNECTED;
    }

    int RakPeer::GetAveragePing(const AddressOrGUID systemIdentifier) {
        std::lock_guard<std::mutex> guard(remoteSystemMutex);

        RemoteSystemStruct *remoteSystem = systemIdentifier.rakNetGuid == UNASSIGNED_RAKNET_GUID
                                           ? GetRemoteSystemFromSystemAddress(systemIdentifier.systemAddress)
                                           : GetRemoteSystemFromGUID(systemIdentifier.rakNetGuid);

        return remoteSystem ? (int) remoteSystem->reliabilityLayer.GetAveragePing() : -1;
    }

    int RakPeer::GetMTUSize(const AddressOrGUID systemIdentifier) {
        std::lock_guard<std::mutex> guard(remoteSystemMutex);

        RemoteSystemStruct *remoteSystem = systemIdentifier.rakNetGuid == UNASSIGNED_RAKNET_GUID
                                           ? GetRemoteSystemFromSystemAddress(systemIdentifier.systemAddress)
                                           : GetRemoteSystemFromGUID(systemIdentifier.rakNetGuid);

        return remoteSystem ? remoteSystem->MTUSize : -1;
    }

    void RakPeer::SetTimeoutTime(TimeMS timeMS, const SystemAddress target) {
        std::lock_guard<std::mutex> guard(remoteSystemMutex);

        if (target == UNASSIGNED_SYSTEM_ADDRESS) {
            for (auto &entry: remoteSystemList)
                entry.second->reliabilityLayer.SetTimeoutTime(timeMS);
            return;
        }

        RemoteSystemStruct *remoteSystem = GetRemoteSystemFromSystemAddress(target);
        if (remoteSystem)
            remoteSystem->reliabilityLayer.SetTimeoutTime(timeMS);
    }

    void RakPeer::SendImmediate(RemoteSystemStruct *remoteSystem, const char *data, unsigned int length,
                                PacketPriority priority, PacketReliability reliability, unsigned char orderingChannel,
                                TimeMS time) {
        remoteSystem->reliabilityLayer.Send(data, BYTES_TO_BITS(length), priority, reliability, orderingChannel, time);
    }

    uint32_t RakPeer::Send(const char *data, int length, PacketPriority priority, PacketReliability reliability,
                           char orderingChannel, const AddressOrGUID systemIdentifier, bool broadcast) {
        if (data == nullptr || length <= 0)
            return 0;

        const TimeMS time = GetTimeMS();

        std::lock_guard<std::mutex> guard(remoteSystemMutex);

        if (broadcast) {
            uint32_t sent = 0;
            for (auto &entry: remoteSystemList) {
                if (entry.second->connectMode != IS_CONNECTED)
                    continue;
                SendImmediate(entry.second.get(), data, (unsigned int) length, priority, reliability,
                              (unsigned char) orderingChannel, time);
                sent++;
            }
            return sent;
        }

        RemoteSystemStruct *remoteSystem = systemIdentifier.rakNetGuid == UNASSIGNED_RAKNET_GUID
                                           ? GetRemoteSystemFromSystemAddress(systemIdentifier.systemAddress)
                                           : GetRemoteSystemFromGUID(systemIdentifier.rakNetGuid);

        if (!remoteSystem || remoteSystem->connectMode != IS_CONNECTED)
            return 0;

        SendImmediate(remoteSystem, data, (unsigned int) length, priority, reliability,
                      (unsigned char) orderingChannel, time);
        return 1;
    }

    void RakPeer::CloseConnection(const AddressOrGUID target, bool sendDisconnectionNotification,
                                  unsigned char orderingChannel, PacketPriority disconnectionNotificationPriority) {
        std::lock_guard<std::mutex> guard(remoteSystemMutex);

        RemoteSystemStruct *remoteSystem = target.rakNetGuid == UNASSIGNED_RAKNET_GUID
                                           ? GetRemoteSystemFromSystemAddress(target.systemAddress)
                                           : GetRemoteSystemFromGUID(target.rakNetGuid);

        if (!remoteSystem)
            return;

        CloseConnectionInternal(remoteSystem->systemAddress, sendDisconnectionNotification, orderingChannel,
                                disconnectionNotificationPriority);
    }

    void RakPeer::CloseConnectionInternal(const SystemAddress &systemAddress, bool sendDisconnectionNotification,
                                          unsigned char orderingChannel, PacketPriority priority) {
        RemoteSystemStruct *remoteSystem = GetRemoteSystemFromSystemAddress(systemAddress);
        if (!remoteSystem)
            return;

        if (sendDisconnectionNotification && remoteSystem->connectMode == IS_CONNECTED) {
            const unsigned char messageId = ID_DISCONNECTION_NOTIFICATION;
            SendImmediate(remoteSystem, (const char *) &messageId, 1, priority, RELIABLE_ORDERED, orderingChannel,
                          GetTimeMS());

            remoteSystem->connectMode = IS_DISCONNECTING;
            remoteSystem->reliabilityLayer.Update(&socket, systemAddress, remoteSystem->MTUSize, GetTimeMS());
        }

        remoteSystemList.erase(systemAddress);
    }

    bool RakPeer::IsOfflineMessageMagic(const unsigned char *data, unsigned int length, unsigned int offset) {
        if (length < offset + sizeof(OFFLINE_MESSAGE_DATA_ID))
            return false;
        return memcmp(data + offset, OFFLINE_MESSAGE_DATA_ID, sizeof(OFFLINE_MESSAGE_DATA_ID)) == 0;
    }

    void RakPeer::SendOfflineMessage(const BitStream &bitStream, const SystemAddress &systemAddress) {
        socket.Send((const char *) bitStream.GetData(), (int) bitStream.GetNumberOfBytesUsed(), systemAddress);
    }

    bool RakPeer::Connect(const char *host, unsigned short remotePort, unsigned int attempts,
                          unsigned int retryIntervalMs) {
        if (endThreads || host == nullptr || attempts == 0)
            return false;

        SystemAddress systemAddress;
        if (!systemAddress.FromString(host, remotePort))
            return false;

        if (systemAddress.GetIPVersion() == 4) {
            const uint32_t rawAddress = systemAddress.addr4.sin_addr.s_addr;

            memset(&systemAddress.addr6, 0, sizeof(systemAddress.addr6));
            systemAddress.addr6.sin6_family = AF_INET6;
            systemAddress.addr6.sin6_port = htons(remotePort);

            unsigned char *bytes = (unsigned char *) &systemAddress.addr6.sin6_addr;
            bytes[10] = 0xff;
            bytes[11] = 0xff;
            memcpy(bytes + 12, &rawAddress, sizeof(rawAddress));
        }

        {
            std::lock_guard<std::mutex> guard(remoteSystemMutex);
            if (GetRemoteSystemFromSystemAddress(systemAddress) != nullptr)
                return false;
        }

        ConnectionAttempt attempt;
        attempt.systemAddress = systemAddress;
        attempt.attemptsLeft = attempts;
        attempt.retryInterval = retryIntervalMs;
        attempt.nextSendTime = GetTimeMS();
        attempt.mtuIndex = 0;

        std::lock_guard<std::mutex> guard(connectionAttemptMutex);
        for (const ConnectionAttempt &existing: connectionAttempts) {
            if (existing.systemAddress == systemAddress)
                return false;
        }

        connectionAttempts.push_back(attempt);
        BitStream ping;
        ping.Write((unsigned char) ID_UNCONNECTED_PING);
        ping.Write((uint64_t) GetTimeMS());
        ping.WriteAlignedBytes(OFFLINE_MESSAGE_DATA_ID, sizeof(OFFLINE_MESSAGE_DATA_ID));
        ping.Write(myGuid.g);
        SendOfflineMessage(ping, systemAddress);
        return true;
    }

    void RakPeer::SendOpenConnectionRequest1(const ConnectionAttempt &attempt) {
        const int mtuSize = CONNECTION_MTU_SIZES[attempt.mtuIndex];

        BitStream out;
        out.Write((unsigned char) ID_OPEN_CONNECTION_REQUEST_1);
        out.WriteAlignedBytes(OFFLINE_MESSAGE_DATA_ID, sizeof(OFFLINE_MESSAGE_DATA_ID));
        out.Write((unsigned char) RAKNET_PROTOCOL_VERSION);

        const int padding = mtuSize - UDP_HEADER_SIZE - (int) out.GetNumberOfBytesUsed();
        for (int index = 0; index < padding; index++)
            out.Write((unsigned char) 0);

        SendOfflineMessage(out, attempt.systemAddress);
    }

    void RakPeer::SendOpenConnectionRequest2(const SystemAddress &systemAddress, uint16_t mtuSize,
                                             bool serverHasSecurity, uint32_t cookie) {
        BitStream out;
        out.Write((unsigned char) ID_OPEN_CONNECTION_REQUEST_2);
        out.WriteAlignedBytes(OFFLINE_MESSAGE_DATA_ID, sizeof(OFFLINE_MESSAGE_DATA_ID));
        if (serverHasSecurity) {
            out.Write(cookie);
            out.Write((unsigned char) 0);
        }
        out.Write(systemAddress);
        out.Write(mtuSize);
        out.Write(myGuid.g);

        SendOfflineMessage(out, systemAddress);
    }

    void RakPeer::FailConnectionAttempt(const SystemAddress &systemAddress, unsigned char reason) {
        {
            std::lock_guard<std::mutex> guard(connectionAttemptMutex);
            for (auto it = connectionAttempts.begin(); it != connectionAttempts.end(); ++it) {
                if (it->systemAddress == systemAddress) {
                    connectionAttempts.erase(it);
                    break;
                }
            }
        }

        Packet *packet = AllocPacket(1, systemAddress, UNASSIGNED_RAKNET_GUID);
        packet->data[0] = reason;
        PushBackPacket(packet);
    }

    void RakPeer::UpdateConnectionAttempts(TimeMS time) {
        std::lock_guard<std::mutex> guard(connectionAttemptMutex);

        for (auto it = connectionAttempts.begin(); it != connectionAttempts.end();) {
            if (time < it->nextSendTime) {
                ++it;
                continue;
            }

            if (it->attemptsLeft == 0) {
                const SystemAddress systemAddress = it->systemAddress;
                it = connectionAttempts.erase(it);

                Packet *packet = AllocPacket(1, systemAddress, UNASSIGNED_RAKNET_GUID);
                packet->data[0] = ID_CONNECTION_ATTEMPT_FAILED;
                PushBackPacket(packet);
                continue;
            }

            SendOpenConnectionRequest1(*it);

            it->attemptsLeft--;
            it->nextSendTime = time + it->retryInterval;
            if (it->mtuIndex + 1 < CONNECTION_MTU_SIZE_COUNT)
                it->mtuIndex++;

            ++it;
        }
    }

    RakPeer::RemoteSystemStruct *RakPeer::AssignSystemAddressToRemoteSystemList(const SystemAddress &systemAddress,
                                                                               int mtuSize, TimeMS time) {
        if (remoteSystemList.size() >= maximumNumberOfPeers)
            return nullptr;

        std::unique_ptr<RemoteSystemStruct> remoteSystem(new RemoteSystemStruct());
        remoteSystem->systemAddress = systemAddress;
        remoteSystem->systemAddress.systemIndex = nextSystemIndex++;
        remoteSystem->MTUSize = mtuSize;
        remoteSystem->connectMode = IS_CONNECTING;
        remoteSystem->connectionTime = time;
        remoteSystem->lastPingTime = time;
        remoteSystem->weStartedTheConnection = false;
        remoteSystem->reliabilityLayer.Reset(mtuSize);

        RemoteSystemStruct *result = remoteSystem.get();
        remoteSystemList[systemAddress] = std::move(remoteSystem);
        return result;
    }

    bool RakPeer::ProcessOfflineNetworkPacket(const RNS2RecvStruct &recvStruct, TimeMS time) {
        const unsigned char *data = (const unsigned char *) recvStruct.data;
        const unsigned int length = (unsigned int) recvStruct.bytesRead;
        const SystemAddress &systemAddress = recvStruct.systemAddress;

        BitStream in((unsigned char *) recvStruct.data, length, false);

        unsigned char messageId;
        if (!in.Read(messageId))
            return false;

        switch (messageId) {
            case ID_UNCONNECTED_PING:
            case ID_UNCONNECTED_PING_OPEN_CONNECTIONS: {
                if (!IsOfflineMessageMagic(data, length, 1 + 8))
                    return false;

                uint64_t pingTime;
                if (!in.Read(pingTime))
                    return false;

                in.IgnoreBytes(sizeof(OFFLINE_MESSAGE_DATA_ID));

                std::lock_guard<std::mutex> guard(offlinePingResponseMutex);

                BitStream out;
                out.Write((unsigned char) ID_UNCONNECTED_PONG);
                out.Write(pingTime);
                out.Write(myGuid.g);
                out.WriteAlignedBytes(OFFLINE_MESSAGE_DATA_ID, sizeof(OFFLINE_MESSAGE_DATA_ID));
                out.Write((uint16_t) offlinePingResponse.size());
                if (!offlinePingResponse.empty())
                    out.WriteAlignedBytes(offlinePingResponse.data(), (unsigned int) offlinePingResponse.size());

                SendOfflineMessage(out, systemAddress);
                return true;
            }

            case ID_OPEN_CONNECTION_REQUEST_1: {
                if (!IsOfflineMessageMagic(data, length, 1))
                    return false;

                in.IgnoreBytes(sizeof(OFFLINE_MESSAGE_DATA_ID));

                unsigned char protocolVersion;
                if (!in.Read(protocolVersion))
                    return false;

                if (protocolVersion != RAKNET_PROTOCOL_VERSION) {
                    BitStream out;
                    out.Write((unsigned char) ID_INCOMPATIBLE_PROTOCOL_VERSION);
                    out.Write((unsigned char) RAKNET_PROTOCOL_VERSION);
                    out.WriteAlignedBytes(OFFLINE_MESSAGE_DATA_ID, sizeof(OFFLINE_MESSAGE_DATA_ID));
                    out.Write(myGuid.g);
                    SendOfflineMessage(out, systemAddress);
                    return true;
                }

                int mtuSize = (int) length + UDP_HEADER_SIZE;
                if (mtuSize < MINIMUM_MTU_SIZE)
                    mtuSize = MINIMUM_MTU_SIZE;
                if (mtuSize > MAXIMUM_INCOMING_MTU_SIZE)
                    mtuSize = MAXIMUM_INCOMING_MTU_SIZE;

                BitStream out;
                out.Write((unsigned char) ID_OPEN_CONNECTION_REPLY_1);
                out.WriteAlignedBytes(OFFLINE_MESSAGE_DATA_ID, sizeof(OFFLINE_MESSAGE_DATA_ID));
                out.Write(myGuid.g);
                // security flag set: the cookie follows, nothing is stored for this address yet
                out.Write((unsigned char) 1);
                out.Write(HandshakeCookie(systemAddress));
                out.Write((uint16_t) mtuSize);

                SendOfflineMessage(out, systemAddress);
                return true;
            }

            case ID_OPEN_CONNECTION_REQUEST_2: {
                if (!IsOfflineMessageMagic(data, length, 1))
                    return false;

                in.IgnoreBytes(sizeof(OFFLINE_MESSAGE_DATA_ID));

                SystemAddress bindingAddress;
                uint16_t mtuSize;
                uint64_t clientGuid;
                uint32_t cookie;
                unsigned char clientSecurity;

                // cookie, then the client's own security byte: a forged source never saw reply 1,
                // so it cannot claim a slot
                if (!in.Read(cookie) || !in.Read(clientSecurity))
                    return false;
                if (cookie != HandshakeCookie(systemAddress))
                    return true;

                if (!in.Read(bindingAddress) || !in.Read(mtuSize) || !in.Read(clientGuid))
                    return false;

                if (mtuSize < MINIMUM_MTU_SIZE)
                    mtuSize = MINIMUM_MTU_SIZE;
                if (mtuSize > MAXIMUM_INCOMING_MTU_SIZE)
                    mtuSize = MAXIMUM_INCOMING_MTU_SIZE;

                std::lock_guard<std::mutex> guard(remoteSystemMutex);

                RemoteSystemStruct *remoteSystem = GetRemoteSystemFromSystemAddress(systemAddress);

                if (remoteSystem && remoteSystem->guid.g == clientGuid) {
                    BitStream out;
                    out.Write((unsigned char) ID_ALREADY_CONNECTED);
                    out.WriteAlignedBytes(OFFLINE_MESSAGE_DATA_ID, sizeof(OFFLINE_MESSAGE_DATA_ID));
                    out.Write(myGuid.g);
                    SendOfflineMessage(out, systemAddress);
                    return true;
                }

                if (remoteSystem)
                    remoteSystemList.erase(systemAddress);

                if (NumberOfConnectionsUnlocked() >= maximumIncomingConnections
                    || !HasHalfOpenRoomFor(systemAddress)) {
                    BitStream out;
                    out.Write((unsigned char) ID_NO_FREE_INCOMING_CONNECTIONS);
                    out.WriteAlignedBytes(OFFLINE_MESSAGE_DATA_ID, sizeof(OFFLINE_MESSAGE_DATA_ID));
                    out.Write(myGuid.g);
                    SendOfflineMessage(out, systemAddress);
                    return true;
                }

                remoteSystem = AssignSystemAddressToRemoteSystemList(systemAddress, mtuSize, time);
                if (!remoteSystem) {
                    BitStream out;
                    out.Write((unsigned char) ID_NO_FREE_INCOMING_CONNECTIONS);
                    out.WriteAlignedBytes(OFFLINE_MESSAGE_DATA_ID, sizeof(OFFLINE_MESSAGE_DATA_ID));
                    out.Write(myGuid.g);
                    SendOfflineMessage(out, systemAddress);
                    return true;
                }

                remoteSystem->guid = RakNetGUID(clientGuid);

                BitStream out;
                out.Write((unsigned char) ID_OPEN_CONNECTION_REPLY_2);
                out.WriteAlignedBytes(OFFLINE_MESSAGE_DATA_ID, sizeof(OFFLINE_MESSAGE_DATA_ID));
                out.Write(myGuid.g);
                out.Write(systemAddress);
                out.Write((uint16_t) mtuSize);
                out.Write((unsigned char) 0);

                SendOfflineMessage(out, systemAddress);
                return true;
            }

            case ID_OPEN_CONNECTION_REPLY_1: {
                if (!IsOfflineMessageMagic(data, length, 1))
                    return false;

                in.IgnoreBytes(sizeof(OFFLINE_MESSAGE_DATA_ID));

                uint64_t serverGuid;
                unsigned char security;
                uint32_t cookie = 0;
                uint16_t mtuSize;

                if (!in.Read(serverGuid) || !in.Read(security))
                    return false;

                if (security != 0 && !in.Read(cookie))
                    return false;

                if (!in.Read(mtuSize))
                    return false;

                if (serverGuid == 0 || mtuSize < MINIMUM_MTU_SIZE || mtuSize > MAXIMUM_ETHERNET_MTU_SIZE) {
                    {
                        std::lock_guard<std::mutex> guard(connectionAttemptMutex);

                        bool known = false;
                        for (const ConnectionAttempt &attempt: connectionAttempts) {
                            if (attempt.systemAddress == systemAddress) {
                                known = true;
                                break;
                            }
                        }

                        if (!known)
                            return true;
                    }

                    SendOpenConnectionRequest2(systemAddress, mtuSize, security != 0, cookie);
                    return true;
                }

                if (mtuSize < MINIMUM_OUTGOING_MTU_SIZE)
                    mtuSize = MINIMUM_OUTGOING_MTU_SIZE;
                if (mtuSize > MAXIMUM_OUTGOING_MTU_SIZE)
                    mtuSize = MAXIMUM_OUTGOING_MTU_SIZE;

                {
                    std::lock_guard<std::mutex> guard(connectionAttemptMutex);

                    bool known = false;
                    for (ConnectionAttempt &attempt: connectionAttempts) {
                        if (attempt.systemAddress == systemAddress) {
                            attempt.nextSendTime = time + attempt.retryInterval;
                            known = true;
                            break;
                        }
                    }

                    if (!known)
                        return true;
                }

                SendOpenConnectionRequest2(systemAddress, mtuSize, security != 0, cookie);
                return true;
            }

            case ID_OPEN_CONNECTION_REPLY_2: {
                if (!IsOfflineMessageMagic(data, length, 1))
                    return false;

                in.IgnoreBytes(sizeof(OFFLINE_MESSAGE_DATA_ID));

                uint64_t serverGuid;
                SystemAddress ourAddress;
                uint16_t mtuSize;
                unsigned char security;

                if (!in.Read(serverGuid) || !in.Read(ourAddress) || !in.Read(mtuSize) || !in.Read(security))
                    return false;

                if (mtuSize < MINIMUM_OUTGOING_MTU_SIZE)
                    mtuSize = MINIMUM_OUTGOING_MTU_SIZE;
                if (mtuSize > MAXIMUM_OUTGOING_MTU_SIZE)
                    mtuSize = MAXIMUM_OUTGOING_MTU_SIZE;

                {
                    std::lock_guard<std::mutex> guard(connectionAttemptMutex);

                    bool known = false;
                    for (auto it = connectionAttempts.begin(); it != connectionAttempts.end(); ++it) {
                        if (it->systemAddress == systemAddress) {
                            connectionAttempts.erase(it);
                            known = true;
                            break;
                        }
                    }

                    if (!known)
                        return true;
                }

                std::lock_guard<std::mutex> guard(remoteSystemMutex);

                if (GetRemoteSystemFromSystemAddress(systemAddress) != nullptr)
                    return true;

                RemoteSystemStruct *remoteSystem = AssignSystemAddressToRemoteSystemList(systemAddress, mtuSize,
                                                                                         time);
                if (remoteSystem == nullptr)
                    return true;

                remoteSystem->guid = RakNetGUID(serverGuid);
                remoteSystem->weStartedTheConnection = true;

                BitStream out;
                out.Write((unsigned char) ID_CONNECTION_REQUEST);
                out.Write(myGuid.g);
                out.Write((uint64_t) time);
                out.Write((unsigned char) 0);

                // ConnectionRequest must not consume the first ordered stream index.
                SendImmediate(remoteSystem, (const char *) out.GetData(), out.GetNumberOfBytesUsed(),
                              IMMEDIATE_PRIORITY, RELIABLE, 0, time);
                return true;
            }

            case ID_ALREADY_CONNECTED:
            case ID_NO_FREE_INCOMING_CONNECTIONS:
            case ID_INCOMPATIBLE_PROTOCOL_VERSION: {
                if (!IsOfflineMessageMagic(data, length, 1) && messageId != ID_INCOMPATIBLE_PROTOCOL_VERSION)
                    return false;

                FailConnectionAttempt(systemAddress, messageId);
                return true;
            }

            default:
                return false;
        }
    }

    void RakPeer::ProcessNetworkPacket(const RNS2RecvStruct &recvStruct, TimeMS time) {
        if (recvStruct.bytesRead <= 0)
            return;

        const unsigned char firstByte = (unsigned char) recvStruct.data[0];

        if ((firstByte & 0x80) == 0) {
            ProcessOfflineNetworkPacket(recvStruct, time);
            return;
        }

        std::lock_guard<std::mutex> guard(remoteSystemMutex);

        RemoteSystemStruct *remoteSystem = GetRemoteSystemFromSystemAddress(recvStruct.systemAddress);
        if (!remoteSystem)
            return;

        ProcessConnectedPacket(remoteSystem, recvStruct, time);
    }

    void RakPeer::ProcessConnectedPacket(RemoteSystemStruct *remoteSystem, const RNS2RecvStruct &recvStruct,
                                         TimeMS time) {
        if (!remoteSystem->reliabilityLayer.HandleSocketReceiveFromConnectedPlayer(recvStruct.data,
                                                                                  (unsigned int) recvStruct.bytesRead,
                                                                                  time))
            return;

        for (;;) {
            InternalPacket *internalPacket = remoteSystem->reliabilityLayer.Receive();
            if (!internalPacket)
                break;

            if (ParseConnectedMessage(remoteSystem, internalPacket, time)) {
                delete internalPacket;
                continue;
            }

            Packet *packet = AllocPacket(BITS_TO_BYTES(internalPacket->dataBitLength), remoteSystem->systemAddress,
                                         remoteSystem->guid);
            memcpy(packet->data, internalPacket->data.data(), packet->length);
            packet->bitSize = internalPacket->dataBitLength;

            delete internalPacket;
            PushBackPacket(packet);
        }
    }

    bool RakPeer::ParseConnectedMessage(RemoteSystemStruct *remoteSystem, InternalPacket *internalPacket, TimeMS time) {
        if (internalPacket->data.empty())
            return true;

        const unsigned char messageId = internalPacket->data[0];

        BitStream in(internalPacket->data.data(), BITS_TO_BYTES(internalPacket->dataBitLength), false);
        in.IgnoreBytes(1);

        switch (messageId) {
            case ID_CONNECTION_REQUEST: {
                uint64_t clientGuid;
                uint64_t requestTime;

                if (!in.Read(clientGuid) || !in.Read(requestTime))
                    return true;

                remoteSystem->guid = RakNetGUID(clientGuid);

                BitStream out;
                out.Write((unsigned char) ID_CONNECTION_REQUEST_ACCEPTED);
                out.Write(remoteSystem->systemAddress);
                out.Write((uint16_t) remoteSystem->systemAddress.systemIndex);

                for (int i = 0; i < MAXIMUM_NUMBER_OF_INTERNAL_IDS; i++)
                    out.Write(UNASSIGNED_SYSTEM_ADDRESS);

                out.Write(requestTime);
                out.Write((uint64_t) time);

                SendImmediate(remoteSystem, (const char *) out.GetData(), out.GetNumberOfBytesUsed(),
                              IMMEDIATE_PRIORITY, RELIABLE_ORDERED, 0, time);
                return true;
            }

            case ID_CONNECTION_REQUEST_ACCEPTED: {
                if (!remoteSystem->weStartedTheConnection || remoteSystem->connectMode == IS_CONNECTED)
                    return true;

                SystemAddress clientAddress;
                uint16_t systemIndex;
                if (!in.Read(clientAddress) || !in.Read(systemIndex))
                    return true;

                for (int i = 0; i < MAXIMUM_NUMBER_OF_INTERNAL_IDS && in.GetNumberOfUnreadBits() > 128; i++) {
                    SystemAddress internalAddress;
                    if (!in.Read(internalAddress))
                        return true;
                }

                uint64_t requestTime;
                uint64_t serverTime;
                if (in.GetNumberOfUnreadBits() != 128 || !in.Read(requestTime) || !in.Read(serverTime))
                    return true;

                remoteSystem->connectMode = IS_CONNECTED;

                BitStream out;
                out.Write((unsigned char) ID_NEW_INCOMING_CONNECTION);
                out.Write(remoteSystem->systemAddress);

                const auto internalAddresses = LocalAddresses(socket.GetBoundAddress());
                for (int i = 0; i < MAXIMUM_NUMBER_OF_INTERNAL_IDS; i++)
                    out.Write(i < internalAddresses.size() ? internalAddresses[i] : UNASSIGNED_SYSTEM_ADDRESS);

                // Some servers use this timestamp as a handshake nonce.
                out.Write(serverTime);
                out.Write((uint64_t) time);

                SendImmediate(remoteSystem, (const char *) out.GetData(), out.GetNumberOfBytesUsed(),
                              IMMEDIATE_PRIORITY, RELIABLE_ORDERED, 0, time);

                remoteSystem->lastPingTime = time - 500;

                Packet *packet = AllocPacket(1, remoteSystem->systemAddress, remoteSystem->guid);
                packet->data[0] = ID_CONNECTION_REQUEST_ACCEPTED;
                PushBackPacket(packet);
                return true;
            }

            case ID_NEW_INCOMING_CONNECTION: {
                if (remoteSystem->connectMode == IS_CONNECTED)
                    return true;

                remoteSystem->connectMode = IS_CONNECTED;

                Packet *packet = AllocPacket(1, remoteSystem->systemAddress, remoteSystem->guid);
                packet->data[0] = ID_NEW_INCOMING_CONNECTION;
                PushBackPacket(packet);
                return true;
            }

            case ID_CONNECTED_PING: {
                uint64_t pingTime;
                if (!in.Read(pingTime))
                    return true;

                BitStream out;
                out.Write((unsigned char) ID_CONNECTED_PONG);
                out.Write(pingTime);
                out.Write((uint64_t) time);

                SendImmediate(remoteSystem, (const char *) out.GetData(), out.GetNumberOfBytesUsed(),
                              IMMEDIATE_PRIORITY, UNRELIABLE, 0, time);
                return true;
            }

            case ID_CONNECTED_PONG:
                return true;

            case ID_DETECT_LOST_CONNECTIONS:
                return true;

            case ID_DISCONNECTION_NOTIFICATION: {
                remoteSystem->connectMode = IS_DISCONNECTED;
                remoteSystem->reliabilityLayer.KillConnection();

                Packet *packet = AllocPacket(1, remoteSystem->systemAddress, remoteSystem->guid);
                packet->data[0] = ID_DISCONNECTION_NOTIFICATION;
                PushBackPacket(packet);
                return true;
            }

            default:
                return false;
        }
    }

    void RakPeer::UpdateNetworkLoop() {
        RNS2RecvStruct recvStruct;

        while (!endThreads) {
            const TimeMS receiveStart = GetTimeMS();
            int waitMs = 5;
            for (int datagrams = 0; datagrams < MAX_DATAGRAMS_PER_UPDATE && socket.RecvFrom(&recvStruct, waitMs); ++datagrams) {
                ProcessNetworkPacket(recvStruct, GetTimeMS());
                waitMs = 0;
                if (GetTimeMS() - receiveStart >= MAX_RECEIVE_MS_PER_UPDATE)
                    break;
            }

            const TimeMS time = GetTimeMS();

            UpdateConnectionAttempts(time);

            std::lock_guard<std::mutex> guard(remoteSystemMutex);

            for (auto it = remoteSystemList.begin(); it != remoteSystemList.end();) {
                RemoteSystemStruct *remoteSystem = it->second.get();
                if (remoteSystem->connectMode == IS_CONNECTED && time - remoteSystem->lastPingTime >= 500) {
                    BitStream ping;
                    ping.Write((unsigned char) ID_CONNECTED_PING);
                    ping.Write((uint64_t) time);
                    SendImmediate(remoteSystem, (const char *) ping.GetData(), ping.GetNumberOfBytesUsed(),
                                  IMMEDIATE_PRIORITY, UNRELIABLE, 0, time);
                    remoteSystem->lastPingTime = time;
                }
                remoteSystem->reliabilityLayer.Update(&socket, remoteSystem->systemAddress, remoteSystem->MTUSize,
                                                      time);


                // a handshake that stalls gives its slot back long before the idle timeout would
                const bool stalled = remoteSystem->connectMode == IS_CONNECTING
                                     && !remoteSystem->weStartedTheConnection
                                     && time - remoteSystem->connectionTime > HALF_OPEN_TIMEOUT_MS;

                if (stalled || remoteSystem->reliabilityLayer.IsDeadConnection()) {
                    if (remoteSystem->connectMode == IS_CONNECTED) {
                        Packet *packet = AllocPacket(1, remoteSystem->systemAddress, remoteSystem->guid);
                        packet->data[0] = ID_CONNECTION_LOST;
                        PushBackPacket(packet);
                    }

                    it = remoteSystemList.erase(it);
                    continue;
                }

                ++it;
            }
        }
    }

}
