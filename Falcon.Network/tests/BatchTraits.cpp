#include "Network/CompressedNetworkPeer.h"

#include <zlib.h>

#include <cstdint>
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

    class WirePeer : public NetworkPeer {
    public:
        void sendPacket(const std::string &data, Reliability, Compressibility) override {
            mIncoming.push_back(data);
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

        bool usesGamePacketId() const override {
            return false;
        }

        std::deque<std::string> mIncoming;
    };

    // text with noise in it: levels differ on it, on a run of one byte they would not
    std::string batchOf(size_t size) {
        std::string batch(size, '\0');
        static const char WORD[] = "minecraft:stone_bricks";
        uint32_t state = 7;
        for (size_t index = 0; index < size; index++) {
            state = state * 1664525u + 1013904223u;
            batch[index] = index % 97 < 60 ? WORD[index % 22] : (char) (state >> 24);
        }
        return batch;
    }

    std::string deflated(const std::string &input, int level) {
        z_stream stream{};
        require(deflateInit2(&stream, level, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) == Z_OK, "deflate did not start");
        std::string out(deflateBound(&stream, (uLong) input.size()), '\0');
        stream.next_in = (Bytef *) input.data();
        stream.avail_in = (uInt) input.size();
        stream.next_out = (Bytef *) &out[0];
        stream.avail_out = (uInt) out.size();
        require(deflate(&stream, Z_FINISH) == Z_STREAM_END, "deflate did not finish");
        out.resize(out.size() - stream.avail_out);
        deflateEnd(&stream);
        return out;
    }

    std::string receive(CompressedNetworkPeer &peer) {
        std::string batch;
        require(peer.receivePacket(batch) == NetworkPeer::DataStatus::HasData, "a batch was lost");
        return batch;
    }
}

int main() {
    try {
        const std::shared_ptr<WirePeer> wire = std::make_shared<WirePeer>();
        CompressedNetworkPeer peer(wire);
        peer.enableCompression(CompressedNetworkPeer::CompressionAlgorithm::ZLib, 256);

        const std::string large = batchOf(6000);
        const std::string small = batchOf(100);
        // a neighbouring level often writes the same bytes: a far one does not
        require(deflated(large, 7) != deflated(large, 1), "the two levels write the same bytes");

        // as the game writes it
        wire->mIncoming.push_back(std::string(1, '\0') + deflated(large, 7));
        require(receive(peer) == large, "the batch changed");
        require(peer.getBatchTraits().mBatches == 1 && peer.getBatchTraits().mCompared == 1, "the batch was not compared");
        require(peer.getBatchTraits().mNotReproduced == 0, "the game's own bytes were not reproduced");
        require(peer.getBatchTraits().mCompressedBelowThreshold == 0, "a long batch counted as short");

        // another level
        wire->mIncoming.push_back(std::string(1, '\0') + deflated(large, 1));
        require(receive(peer) == large, "the batch changed");
        require(peer.getBatchTraits().mNotReproduced == 1, "another level was taken for the game's");

        // a short batch the game would send as it is
        wire->mIncoming.push_back(std::string(1, '\0') + deflated(small, 7));
        require(receive(peer) == small, "the batch changed");
        require(peer.getBatchTraits().mCompressedBelowThreshold == 1, "a short compressed batch went unseen");

        // sent as it is: nothing to count
        wire->mIncoming.push_back(std::string(1, '\xff') + small);
        require(receive(peer) == small, "the batch changed");
        require(peer.getBatchTraits().mBatches == 3, "a batch sent as it is was counted");

        // the other algorithm
        std::string snappy;
        require(CompressedNetworkPeer::compress(CompressedNetworkPeer::CompressionAlgorithm::Snappy, large, snappy),
                "snappy did not compress");
        wire->mIncoming.push_back(std::string(1, '\x01') + snappy);
        require(receive(peer) == large, "the batch changed");
        require(peer.getBatchTraits().mOtherAlgorithm == 1, "the other algorithm went unseen");
    } catch (const std::exception &error) {
        std::cout << "FAIL: " << error.what() << std::endl;
        return 1;
    }

    std::cout << "PASS: batch traits" << std::endl;
    return 0;
}
