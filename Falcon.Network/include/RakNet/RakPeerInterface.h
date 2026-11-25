#pragma once

#include "RakNet/PacketPriority.h"
#include "RakNet/RakNetTypes.h"

#include <string>

namespace RakNet {

    class RakPeerInterface {
    public:
        virtual ~RakPeerInterface() = default;

        virtual StartupResult Startup(unsigned int maxConnections, SocketDescriptor *socketDescriptors,
                                      unsigned int socketDescriptorCount) = 0;

        virtual bool Connect(const char *host, unsigned short remotePort, unsigned int attempts = 12,
                             unsigned int retryIntervalMs = 500) = 0;

        virtual void Shutdown(unsigned int blockDuration) = 0;

        virtual bool IsActive() const = 0;

        virtual void SetMaximumIncomingConnections(unsigned short numberAllowed) = 0;

        virtual unsigned short GetMaximumIncomingConnections() const = 0;

        virtual unsigned short NumberOfConnections() const = 0;

        virtual uint32_t Send(const char *data, int length, PacketPriority priority, PacketReliability reliability,
                              char orderingChannel, const AddressOrGUID systemIdentifier, bool broadcast) = 0;

        virtual Packet *Receive() = 0;

        virtual void DeallocatePacket(Packet *packet) = 0;

        virtual void CloseConnection(const AddressOrGUID target, bool sendDisconnectionNotification,
                                     unsigned char orderingChannel = 0,
                                     PacketPriority disconnectionNotificationPriority = LOW_PRIORITY) = 0;

        virtual ConnectionState GetConnectionState(const AddressOrGUID systemIdentifier) = 0;

        virtual void SetOfflinePingResponse(const char *data, unsigned int length) = 0;

        virtual RakNetGUID GetMyGUID() const = 0;

        virtual SystemAddress GetMyBoundAddress() const = 0;

        virtual SystemAddress GetSystemAddressFromGuid(const RakNetGUID input) const = 0;

        virtual RakNetGUID GetGuidFromSystemAddress(const SystemAddress input) const = 0;

        virtual int GetAveragePing(const AddressOrGUID systemIdentifier) = 0;

        virtual int GetMTUSize(const AddressOrGUID systemIdentifier) = 0;

        virtual void SetTimeoutTime(TimeMS timeMS, const SystemAddress target) = 0;

        static RakPeerInterface *GetInstance();

        static void DestroyInstance(RakPeerInterface *instance);
    };

}
