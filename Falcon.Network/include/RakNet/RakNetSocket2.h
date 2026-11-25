#pragma once

#include "RakNet/RakNetTypes.h"

namespace RakNet {

    struct RNS2RecvStruct {
        // Receive both connection directions without truncating incoming datagrams.
        char data[MAXIMUM_INCOMING_MTU_SIZE];
        int bytesRead;
        SystemAddress systemAddress;
        TimeUS timeRead;
    };

    enum RNS2BindResult {
        BR_SUCCESS,
        BR_REQUIRES_RAKNET_SUPPORT_IPV6_DEFINE,
        BR_FAILED_TO_BIND_SOCKET,
        BR_FAILED_SEND_TEST
    };

    class RakNetSocket2 {
    public:
        RakNetSocket2();

        ~RakNetSocket2();

        RakNetSocket2(const RakNetSocket2 &) = delete;

        RakNetSocket2 &operator=(const RakNetSocket2 &) = delete;

        RNS2BindResult Bind(const char *hostAddress, unsigned short port, short socketFamily);

        void Close();

        bool IsOpen() const;

        int Send(const char *data, int length, const SystemAddress &systemAddress);

        bool RecvFrom(RNS2RecvStruct *recvStruct, int timeoutMs);

        const SystemAddress &GetBoundAddress() const { return boundAddress; }

    private:
        int sock;
        SystemAddress boundAddress;
    };

}
