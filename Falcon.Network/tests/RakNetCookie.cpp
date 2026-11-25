#include "RakNet/RakPeer.h"
#include "RakNet/MessageIdentifiers.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
    using namespace RakNet;

    constexpr uint16_t Mtu = 1200;

    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void sendRequest1(RakNetSocket2 &socket, const SystemAddress &server) {
        BitStream out;
        out.Write((unsigned char) ID_OPEN_CONNECTION_REQUEST_1);
        out.WriteAlignedBytes(OFFLINE_MESSAGE_DATA_ID, sizeof(OFFLINE_MESSAGE_DATA_ID));
        out.Write((unsigned char) RAKNET_PROTOCOL_VERSION);
        for (int index = 0; index < 600; index++)
            out.Write((unsigned char) 0);
        socket.Send((const char *) out.GetData(), (int) out.GetNumberOfBytesUsed(), server);
    }

    void sendRequest2(RakNetSocket2 &socket, const SystemAddress &server, uint32_t cookie, uint64_t guid) {
        BitStream out;
        out.Write((unsigned char) ID_OPEN_CONNECTION_REQUEST_2);
        out.WriteAlignedBytes(OFFLINE_MESSAGE_DATA_ID, sizeof(OFFLINE_MESSAGE_DATA_ID));
        out.Write(cookie);
        out.Write((unsigned char) 0);
        out.Write(server);
        out.Write(Mtu);
        out.Write(guid);
        socket.Send((const char *) out.GetData(), (int) out.GetNumberOfBytesUsed(), server);
    }

    // first offline reply within the wait, 0 when the server stays silent
    unsigned char receive(RakNetSocket2 &socket, RNS2RecvStruct &received, int waitMs) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(waitMs);
        while (std::chrono::steady_clock::now() < deadline) {
            if (socket.RecvFrom(&received, 10) && received.bytesRead > 0)
                return (unsigned char) received.data[0];
        }
        return 0;
    }

    uint32_t requestCookie(RakNetSocket2 &socket, const SystemAddress &server) {
        sendRequest1(socket, server);
        RNS2RecvStruct received;
        require(receive(socket, received, 1000) == ID_OPEN_CONNECTION_REPLY_1, "no reply 1");

        BitStream in((unsigned char *) received.data, (unsigned int) received.bytesRead, false);
        in.IgnoreBytes(1 + sizeof(OFFLINE_MESSAGE_DATA_ID));
        uint64_t guid;
        unsigned char security;
        uint32_t cookie;
        require(in.Read(guid) && in.Read(security) && security == 1 && in.Read(cookie),
                "reply 1 carries no cookie");
        return cookie;
    }

    std::unique_ptr<RakPeer> startServer(SystemAddress &address) {
        std::unique_ptr<RakPeer> server(new RakPeer());
        SocketDescriptor descriptor(0, "127.0.0.1");
        descriptor.socketFamily = AF_INET;
        require(server->Startup(64, &descriptor, 1) == RAKNET_STARTED, "start server");
        server->SetMaximumIncomingConnections(64);
        address = SystemAddress("127.0.0.1", server->GetMyBoundAddress().GetPort());
        return server;
    }

    void checkCookieIsRequired() {
        SystemAddress address;
        const std::unique_ptr<RakPeer> server = startServer(address);
        RakNetSocket2 client;
        require(client.Bind("127.0.0.1", 0, AF_INET) == BR_SUCCESS, "bind client");

        RNS2RecvStruct received;
        sendRequest2(client, address, 0x12345678u, 1);
        require(receive(client, received, 300) == 0, "a request without the cookie got an answer");

        const uint32_t cookie = requestCookie(client, address);
        sendRequest2(client, address, cookie ^ 1u, 1);
        require(receive(client, received, 300) == 0, "a wrong cookie got an answer");

        sendRequest2(client, address, cookie, 1);
        require(receive(client, received, 1000) == ID_OPEN_CONNECTION_REPLY_2, "the right cookie was refused");
    }

    void checkHalfOpenLimit() {
        SystemAddress address;
        const std::unique_ptr<RakPeer> server = startServer(address);

        std::vector<std::unique_ptr<RakNetSocket2>> clients;
        unsigned char last = 0;
        for (int index = 0; index < 9; index++) {
            clients.emplace_back(new RakNetSocket2());
            require(clients.back()->Bind("127.0.0.1", 0, AF_INET) == BR_SUCCESS, "bind client");
            const uint32_t cookie = requestCookie(*clients.back(), address);
            sendRequest2(*clients.back(), address, cookie, (uint64_t) index + 1);
            RNS2RecvStruct received;
            last = receive(*clients.back(), received, 1000);
            if (index < 8)
                require(last == ID_OPEN_CONNECTION_REPLY_2, "a handshake under the limit was refused");
        }
        require(last == ID_NO_FREE_INCOMING_CONNECTIONS, "one address held more handshakes than allowed");
    }

    void checkPeersStillConnect() {
        SystemAddress address;
        const std::unique_ptr<RakPeer> server = startServer(address);
        RakPeer client;
        SocketDescriptor descriptor(0, "::");
        descriptor.socketFamily = AF_INET6;
        require(client.Startup(1, &descriptor, 1) == RAKNET_STARTED, "start client");
        require(client.Connect("127.0.0.1", address.GetPort()), "connect client");

        bool accepted = false;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!accepted && std::chrono::steady_clock::now() < deadline) {
            while (Packet *packet = client.Receive()) {
                if (packet->data[0] == ID_CONNECTION_REQUEST_ACCEPTED)
                    accepted = true;
                client.DeallocatePacket(packet);
            }
            while (Packet *packet = server->Receive())
                server->DeallocatePacket(packet);
        }
        require(accepted, "two peers no longer connect");
    }
}

int main() {
    try {
        checkCookieIsRequired();
        checkHalfOpenLimit();
        checkPeersStillConnect();
    } catch (const std::exception &error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
