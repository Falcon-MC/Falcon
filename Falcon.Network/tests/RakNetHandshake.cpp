#include "RakNet/RakPeer.h"
#include "RakNet/MessageIdentifiers.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
    constexpr uint16_t Mtu = 1200;
    constexpr uint64_t ServerGuid = 0x123456789;
    constexpr uint64_t ServerTime = 0x456789abcdef;

    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void checkHandshake(int addressCount, bool ipv6, int sizeAdjustment) {
        using namespace RakNet;
        RakNetSocket2 server;
        require(server.Bind("127.0.0.1", 0, AF_INET) == BR_SUCCESS, "bind fixture");
        RakPeer client;
        SocketDescriptor descriptor(0, "::");
        descriptor.socketFamily = AF_INET6;
        require(client.Startup(1, &descriptor, 1) == RAKNET_STARTED, "start client");
        require(client.Connect("127.0.0.1", server.GetBoundAddress().GetPort()), "connect client");

        ReliabilityLayer reliability;
        reliability.Reset(Mtu);
        SystemAddress address;
        bool acceptedSent = false;
        bool incomingReceived = false;
        bool connectedReported = false;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < deadline) {
            RNS2RecvStruct received;
            if (server.RecvFrom(&received, 5)) {
                address = received.systemAddress;
                const auto id = static_cast<unsigned char>(received.data[0]);
                BitStream reply;
                if (id == ID_OPEN_CONNECTION_REQUEST_1 || id == ID_OPEN_CONNECTION_REQUEST_2) {
                    reply.Write(static_cast<unsigned char>(id == ID_OPEN_CONNECTION_REQUEST_1
                        ? ID_OPEN_CONNECTION_REPLY_1 : ID_OPEN_CONNECTION_REPLY_2));
                    reply.WriteAlignedBytes(OFFLINE_MESSAGE_DATA_ID, sizeof(OFFLINE_MESSAGE_DATA_ID));
                    reply.Write(ServerGuid);
                    if (id == ID_OPEN_CONNECTION_REQUEST_1) {
                        reply.Write(static_cast<unsigned char>(0));
                        reply.Write(Mtu);
                    } else {
                        reply.Write(address);
                        reply.Write(Mtu);
                        reply.Write(static_cast<unsigned char>(0));
                    }
                    server.Send(reinterpret_cast<const char *>(reply.GetData()), reply.GetNumberOfBytesUsed(), address);
                } else if (id & 0x80) {
                    require(reliability.HandleSocketReceiveFromConnectedPlayer(received.data, received.bytesRead,
                        GetTimeMS()), "decode datagram");
                    while (auto raw = reliability.Receive()) {
                        std::unique_ptr<InternalPacket> packet(raw);
                        if (packet->data[0] == ID_CONNECTION_REQUEST && !acceptedSent) {
                            require(packet->reliability == RELIABLE,
                                "connection request consumed an ordered stream index");
                            BitStream request(packet->data.data(), BITS_TO_BYTES(packet->dataBitLength), false);
                            request.IgnoreBytes(1);
                            uint64_t guid, requestTime;
                            require(request.Read(guid) && request.Read(requestTime), "decode request");
                            reply.Write(static_cast<unsigned char>(ID_CONNECTION_REQUEST_ACCEPTED));
                            reply.Write(address);
                            reply.Write(static_cast<uint16_t>(0));
                            for (int i = 0; i < addressCount; ++i)
                                reply.Write(ipv6 && i % 2 == 0
                                    ? SystemAddress("::1", 1234) : UNASSIGNED_SYSTEM_ADDRESS);
                            reply.Write(requestTime);
                            reply.Write(ServerTime);
                            if (sizeAdjustment > 0)
                                reply.Write(static_cast<unsigned char>(0));
                            else if (sizeAdjustment < 0)
                                reply.SetWriteOffset(reply.GetNumberOfBitsUsed() - 8);
                            require(reliability.Send(reinterpret_cast<const char *>(reply.GetData()),
                                reply.GetNumberOfBitsUsed(), IMMEDIATE_PRIORITY, RELIABLE_ORDERED, 0, GetTimeMS()),
                                "send accepted");
                            acceptedSent = true;
                            if (sizeAdjustment != 0 || addressCount > MAXIMUM_NUMBER_OF_INTERNAL_IDS)
                                deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
                        } else if (packet->data[0] == ID_NEW_INCOMING_CONNECTION) {
                            require(packet->reliability == RELIABLE_ORDERED && packet->orderingIndex == 0,
                                "new incoming connection did not start the ordered stream");
                            require(sizeAdjustment == 0 && addressCount <= MAXIMUM_NUMBER_OF_INTERNAL_IDS,
                                "malformed acceptance was accepted");
                            BitStream incoming(packet->data.data(), BITS_TO_BYTES(packet->dataBitLength), false);
                            incoming.SetReadOffset(packet->dataBitLength - 128);
                            uint64_t echoTime, localTime;
                            require(incoming.Read(echoTime) && incoming.Read(localTime), "decode incoming times");
                            require(echoTime == ServerTime, "server timestamp was not echoed");
                            require(localTime != ServerTime, "local timestamp replaced by server timestamp");
                            incomingReceived = true;
                        }
                    }
                }
            }
            if (!address.IsUnassigned())
                reliability.Update(&server, address, Mtu, GetTimeMS());
            while (auto packet = client.Receive()) {
                if (packet->data[0] == ID_CONNECTION_REQUEST_ACCEPTED)
                    connectedReported = true;
                client.DeallocatePacket(packet);
            }
            if (incomingReceived && connectedReported)
                break;
        }
        require(acceptedSent, "fixture never received connection request");
        const bool valid = sizeAdjustment == 0 && addressCount <= MAXIMUM_NUMBER_OF_INTERNAL_IDS;
        require(incomingReceived == valid, "incoming connection result differs");
        require(connectedReported == valid, "client reported wrong connection state");
    }
}

int main() {
    try {
        checkHandshake(0, false, 0);
        checkHandshake(10, false, 0);
        checkHandshake(20, false, 0);
        checkHandshake(20, true, 0);
        checkHandshake(20, false, -1);
        checkHandshake(20, false, 1);
        checkHandshake(21, false, 0);
        std::cout << "RakNet timestamp echo and malformed acceptance tests passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
