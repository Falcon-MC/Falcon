#pragma once

#include "RakNet/RakNetSocket2.h"
#include "RakNet/RakPeerInterface.h"
#include "RakNet/ReliabilityLayer.h"

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace RakNet {

    class RakPeer : public RakPeerInterface {
    public:
        struct RemoteSystemStruct {
            SystemAddress systemAddress;
            RakNetGUID guid;
            ReliabilityLayer reliabilityLayer;
            int MTUSize;
            ConnectionState connectMode;
            TimeMS connectionTime;
            TimeMS lastPingTime;
            bool weStartedTheConnection;

            RemoteSystemStruct()
                    : MTUSize(MAXIMUM_MTU_SIZE), connectMode(IS_PENDING), connectionTime(0), lastPingTime(0),
                      weStartedTheConnection(false) {}
        };

        RakPeer();

        ~RakPeer() override;

        StartupResult Startup(unsigned int maxConnections, SocketDescriptor *socketDescriptors,
                              unsigned int socketDescriptorCount) override;

        bool Connect(const char *host, unsigned short remotePort, unsigned int attempts = 12,
                     unsigned int retryIntervalMs = 500) override;

        void Shutdown(unsigned int blockDuration) override;

        bool IsActive() const override { return endThreads == false; }

        void SetMaximumIncomingConnections(unsigned short numberAllowed) override;

        unsigned short GetMaximumIncomingConnections() const override { return maximumIncomingConnections; }

        unsigned short NumberOfConnections() const override;

        uint32_t Send(const char *data, int length, PacketPriority priority, PacketReliability reliability,
                      char orderingChannel, const AddressOrGUID systemIdentifier, bool broadcast) override;

        Packet *Receive() override;

        void DeallocatePacket(Packet *packet) override;

        void CloseConnection(const AddressOrGUID target, bool sendDisconnectionNotification,
                             unsigned char orderingChannel = 0,
                             PacketPriority disconnectionNotificationPriority = LOW_PRIORITY) override;

        ConnectionState GetConnectionState(const AddressOrGUID systemIdentifier) override;

        void SetOfflinePingResponse(const char *data, unsigned int length) override;

        RakNetGUID GetMyGUID() const override { return myGuid; }

        SystemAddress GetMyBoundAddress() const override { return socket.GetBoundAddress(); }

        SystemAddress GetSystemAddressFromGuid(const RakNetGUID input) const override;

        RakNetGUID GetGuidFromSystemAddress(const SystemAddress input) const override;

        int GetAveragePing(const AddressOrGUID systemIdentifier) override;

        int GetMTUSize(const AddressOrGUID systemIdentifier) override;

        void SetTimeoutTime(TimeMS timeMS, const SystemAddress target) override;

    private:
        struct ConnectionAttempt {
            SystemAddress systemAddress;
            TimeMS nextSendTime;
            unsigned int attemptsLeft;
            unsigned int retryInterval;
            size_t mtuIndex;

            ConnectionAttempt() : nextSendTime(0), attemptsLeft(0), retryInterval(0), mtuIndex(0) {}
        };

        typedef std::unordered_map<SystemAddress, std::unique_ptr<RemoteSystemStruct>, SystemAddressHash> RemoteSystemList;

        void UpdateNetworkLoop();

        void UpdateConnectionAttempts(TimeMS time);

        void SendOpenConnectionRequest1(const ConnectionAttempt &attempt);

        void SendOpenConnectionRequest2(const SystemAddress &systemAddress, uint16_t mtuSize, bool serverHasSecurity,
                                        uint32_t cookie);

        void FailConnectionAttempt(const SystemAddress &systemAddress, unsigned char reason);

        void ProcessNetworkPacket(const RNS2RecvStruct &recvStruct, TimeMS time);

        bool ProcessOfflineNetworkPacket(const RNS2RecvStruct &recvStruct, TimeMS time);

        void ProcessConnectedPacket(RemoteSystemStruct *remoteSystem, const RNS2RecvStruct &recvStruct, TimeMS time);

        bool ParseConnectedMessage(RemoteSystemStruct *remoteSystem, InternalPacket *internalPacket, TimeMS time);

        unsigned short NumberOfConnectionsUnlocked() const;

        RemoteSystemStruct *GetRemoteSystemFromSystemAddress(const SystemAddress &systemAddress) const;

        RemoteSystemStruct *GetRemoteSystemFromGUID(const RakNetGUID &guid) const;

        RemoteSystemStruct *AssignSystemAddressToRemoteSystemList(const SystemAddress &systemAddress, int mtuSize,
                                                                  TimeMS time);

        void CloseConnectionInternal(const SystemAddress &systemAddress, bool sendDisconnectionNotification,
                                     unsigned char orderingChannel, PacketPriority priority);

        void SendImmediate(RemoteSystemStruct *remoteSystem, const char *data, unsigned int length,
                           PacketPriority priority, PacketReliability reliability, unsigned char orderingChannel,
                           TimeMS time);

        void SendOfflineMessage(const BitStream &bitStream, const SystemAddress &systemAddress);

        void PushBackPacket(Packet *packet);

        Packet *AllocPacket(unsigned int dataSize, const SystemAddress &systemAddress, const RakNetGUID &guid);

        static bool IsOfflineMessageMagic(const unsigned char *data, unsigned int length, unsigned int offset);

        // what request 2 must echo: only a peer that received reply 1 at this address can
        uint32_t HandshakeCookie(const SystemAddress &systemAddress) const;

        // whether one more handshake from this address fits, per host and overall; remoteSystemMutex held
        bool HasHalfOpenRoomFor(const SystemAddress &systemAddress) const;

        RakNetSocket2 socket;
        std::thread networkThread;
        std::atomic<bool> endThreads;

        mutable std::mutex remoteSystemMutex;
        RemoteSystemList remoteSystemList;

        std::mutex packetQueueMutex;
        std::deque<Packet *> packetQueue;

        std::vector<unsigned char> offlinePingResponse;
        std::mutex offlinePingResponseMutex;

        mutable std::mutex connectionAttemptMutex;
        std::vector<ConnectionAttempt> connectionAttempts;

        unsigned char cookieSecret[32];
        RakNetGUID myGuid;
        unsigned int maximumNumberOfPeers;
        unsigned short maximumIncomingConnections;
        SystemIndex nextSystemIndex;
    };

}
