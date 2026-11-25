#include "Network/EncryptedNetworkPeer.h"

#include <deque>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // hands back what it is given, and keeps what is sent through it
    class LoopPeer : public NetworkPeer {
    public:
        void sendPacket(const std::string &data, Reliability, Compressibility) override {
            mSent.push_back(data);
        }

        DataStatus receivePacket(std::string &outData) override {
            if (mIncoming.empty())
                return DataStatus::NoData;
            outData = mIncoming.front();
            mIncoming.pop_front();
            return DataStatus::HasData;
        }

        NetworkStatus getNetworkStatus() const override {
            return NetworkStatus();
        }

        std::deque<std::string> mIncoming;
        std::deque<std::string> mSent;
    };

    const std::string PLAIN = std::string("\xfe", 1) + "a clear packet sent without the key";

    EncryptionKey key() {
        EncryptionKey value{};
        for (size_t index = 0; index < value.size(); ++index)
            value[index] = (uint8_t) (index * 7 + 3);
        return value;
    }

    // what a peer holding the key puts on the wire for one packet
    std::string encrypted(const std::string &packet) {
        const std::shared_ptr<LoopPeer> wire = std::make_shared<LoopPeer>();
        EncryptedNetworkPeer sender(wire);
        require(sender.enableEncryption(key()), "the sending side could not enable encryption");
        sender.sendPacket(packet, NetworkPeer::Reliability::ReliableOrdered, Compressibility::Compressible);
        require(wire->mSent.size() == 1, "the sending side sent nothing");
        return wire->mSent.front();
    }

    void run() {
        std::string received;

        // a server: once the handshake switched to encryption, a clear packet ends the connection
        {
            const std::shared_ptr<LoopPeer> wire = std::make_shared<LoopPeer>();
            EncryptedNetworkPeer server(wire);
            require(server.enableEncryption(key()), "the server could not enable encryption");
            wire->mIncoming.push_back(PLAIN);
            require(server.receivePacket(received) == NetworkPeer::DataStatus::NoData && server.hasFailed(),
                    "a server read a clear packet after the encryption handshake");
        }

        // a server still reads what the key holder sends
        {
            const std::shared_ptr<LoopPeer> wire = std::make_shared<LoopPeer>();
            EncryptedNetworkPeer server(wire);
            require(server.enableEncryption(key()), "the server could not enable encryption");
            wire->mIncoming.push_back(encrypted(PLAIN));
            require(server.receivePacket(received) == NetworkPeer::DataStatus::HasData && received == PLAIN
                            && !server.hasFailed(),
                    "a server refused a packet encrypted with its key");
        }

        // a client keeps the grace for one clear packet flushed by the server, then reads encrypted ones
        {
            const std::shared_ptr<LoopPeer> wire = std::make_shared<LoopPeer>();
            EncryptedNetworkPeer client(wire, true);
            require(client.enableEncryption(key()), "the client could not enable encryption");
            wire->mIncoming.push_back(PLAIN);
            wire->mIncoming.push_back(encrypted(PLAIN));
            require(client.receivePacket(received) == NetworkPeer::DataStatus::HasData && received == PLAIN,
                    "a client lost the clear packet flushed after the handshake");
            require(client.receivePacket(received) == NetworkPeer::DataStatus::HasData && received == PLAIN
                            && !client.hasFailed(),
                    "a client could not read the first encrypted packet");
        }
    }
}

int main() {
    try {
        run();
    } catch (const std::exception &error) {
        std::cerr << "Encrypted peer regression failed: " << error.what() << std::endl;
        return 1;
    }
    return 0;
}
