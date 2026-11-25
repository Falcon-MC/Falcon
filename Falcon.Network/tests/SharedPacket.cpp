#include "Network/BatchedNetworkPeer.h"
#include "Network/CompressedNetworkPeer.h"
#include "Network/SharedPacket.h"

#include <deque>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // what one side sends, the other receives
    class LoopPeer : public NetworkPeer {
    public:
        explicit LoopPeer(bool gamePacketId) : mGamePacketId(gamePacketId) {}

        void sendPacket(const std::string &data, Reliability, Compressibility) override {
            mSent.push_back(data);
        }

        DataStatus receivePacket(std::string &outData) override {
            if (mSent.empty())
                return DataStatus::NoData;

            outData = mSent.front();
            mSent.pop_front();
            return DataStatus::HasData;
        }

        NetworkStatus getNetworkStatus() const override {
            return NetworkStatus();
        }

        bool usesGamePacketId() const override {
            return mGamePacketId;
        }

        std::deque<std::string> mSent;

    private:
        bool mGamePacketId;
    };

    std::vector<std::string> receiveAll(BatchedNetworkPeer &peer) {
        std::vector<std::string> packets;
        std::string packet;
        while (peer.receivePacket(packet) == NetworkPeer::DataStatus::HasData)
            packets.push_back(packet);
        return packets;
    }

    void roundTrip(bool gamePacketId, CompressedNetworkPeer::CompressionAlgorithm algorithm, unsigned short threshold) {
        const std::shared_ptr<LoopPeer> wire = std::make_shared<LoopPeer>(gamePacketId);
        const std::shared_ptr<CompressedNetworkPeer> compressed = std::make_shared<CompressedNetworkPeer>(wire);
        compressed->enableCompression(algorithm, threshold);
        BatchedNetworkPeer batched(compressed);

        const std::string chunk(5000, 'c');
        const SharedPacket shared(chunk);
        batched.sendPacket("before", NetworkPeer::Reliability::ReliableOrdered, Compressibility::Compressible);
        batched.sendShared(shared, NetworkPeer::Reliability::ReliableOrdered);
        batched.sendPacket("after", NetworkPeer::Reliability::ReliableOrdered, Compressibility::Compressible);
        batched.flush();
        require(wire->mSent.size() == 3, "the shared packet did not get a batch of its own");

        const std::vector<std::string> packets = receiveAll(batched);
        require(packets.size() == 3, "a packet was lost");
        require(packets[0] == "before" && packets[1] == chunk && packets[2] == "after", "packet order changed");
    }

    void compressesOnce() {
        const SharedPacket shared(std::string(4096, 'x'));
        const std::string &first = shared.getCompressed(CompressedNetworkPeer::CompressionAlgorithm::ZLib);
        const std::string &second = shared.getCompressed(CompressedNetworkPeer::CompressionAlgorithm::ZLib);
        require(!first.empty() && &first == &second, "the compressed form was not kept");
        require(first.size() < shared.getBatch().size(), "the batch was not compressed");
        require(shared.getCompressed(CompressedNetworkPeer::CompressionAlgorithm::None).empty(),
                "an uncompressed algorithm got a compressed form");
        require(shared.getPacket() == std::string(4096, 'x'), "the packet bytes changed");
    }
}

int main() {
    try {
        compressesOnce();
        roundTrip(false, CompressedNetworkPeer::CompressionAlgorithm::ZLib, 1);
        roundTrip(true, CompressedNetworkPeer::CompressionAlgorithm::ZLib, 1);
        roundTrip(false, CompressedNetworkPeer::CompressionAlgorithm::Snappy, 1);
        roundTrip(false, CompressedNetworkPeer::CompressionAlgorithm::ZLib, 60000);
    } catch (const std::exception &error) {
        std::cout << "FAIL: " << error.what() << std::endl;
        return 1;
    }

    std::cout << "PASS: shared packet" << std::endl;
    return 0;
}
