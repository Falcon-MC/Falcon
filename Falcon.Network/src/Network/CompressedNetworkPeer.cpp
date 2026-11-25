#include "Network/CompressedNetworkPeer.h"

#include "Network/SharedPacket.h"

#include <snappy.h>
#include <zlib.h>

#include <cstdint>

namespace {

    const int RAW_DEFLATE_WINDOW_BITS = -15;
    const size_t MAX_DECOMPRESSED_SIZE = 8 * 1024 * 1024;

    // the game deflates a batch at level 7, raw, memory level 8, in one go
    const int GAME_DEFLATE_LEVEL = 7;
    const int GAME_MEMORY_LEVEL = 8;
    // the first batches say enough, and a large one costs too much to deflate again
    const unsigned short OBSERVED_BATCHES = 64;
    const size_t MAX_COMPARED_SIZE = 256 * 1024;

    bool deflateLikeGame(const char *data, size_t length, std::string &outData) {
        z_stream stream{};
        if (deflateInit2(&stream, GAME_DEFLATE_LEVEL, Z_DEFLATED, RAW_DEFLATE_WINDOW_BITS, GAME_MEMORY_LEVEL,
                         Z_DEFAULT_STRATEGY)
            != Z_OK)
            return false;

        outData.resize(deflateBound(&stream, (uLong) length));
        stream.next_in = (Bytef *) data;
        stream.avail_in = (uInt) length;
        stream.next_out = (Bytef *) &outData[0];
        stream.avail_out = (uInt) outData.size();
        const int result = deflate(&stream, Z_FINISH);
        const size_t written = outData.size() - stream.avail_out;
        deflateEnd(&stream);
        if (result != Z_STREAM_END)
            return false;
        outData.resize(written);
        return true;
    }

    // a zlib that is not the reference one (a drop-in replacement) writes other bytes for the same input:
    // comparing against it would say nothing
    bool deflatesLikeReference() {
        static const bool same = []() {
            std::string input(20000, '\0');
            static const char WORD[] = "minecraft:stone_bricks";
            uint32_t state = 0x12345678;
            for (size_t index = 0; index < input.size(); index++) {
                state = state * 1664525u + 1013904223u;
                input[index] = index % 97 < 60 ? WORD[index % 22] : (char) (state >> 24);
            }

            std::string output;
            if (!deflateLikeGame(input.data(), input.size(), output) || output.size() != 8269)
                return false;
            uint32_t hash = 2166136261u;
            for (const char byte: output) {
                hash ^= (unsigned char) byte;
                hash *= 16777619u;
            }
            // zlib 1.3.1, the version the game is built with
            return hash == 0x55829a2c;
        }();
        return same;
    }

}

CompressedNetworkPeer::CompressedNetworkPeer(std::shared_ptr<NetworkPeer> peer)
        : mPeer(std::move(peer)), mAlgorithm(CompressionAlgorithm::ZLib), mCompressionThreshold(1),
          mCompressionEnabled(false) {}

void CompressedNetworkPeer::enableCompression(CompressionAlgorithm algorithm, unsigned short threshold) {
    mAlgorithm = algorithm;
    mCompressionThreshold = threshold;
    mCompressionEnabled = true;
}

void CompressedNetworkPeer::disableCompression() {
    mCompressionEnabled = false;
}

bool CompressedNetworkPeer::_compressZlib(const std::string &input, std::string &outData) {
    z_stream stream{};

    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, RAW_DEFLATE_WINDOW_BITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return false;

    outData.resize(deflateBound(&stream, (uLong) input.size()));

    stream.next_in = (Bytef *) input.data();
    stream.avail_in = (uInt) input.size();
    stream.next_out = (Bytef *) &outData[0];
    stream.avail_out = (uInt) outData.size();

    const int result = deflate(&stream, Z_FINISH);
    const size_t written = outData.size() - stream.avail_out;
    deflateEnd(&stream);

    if (result != Z_STREAM_END)
        return false;

    outData.resize(written);
    return true;
}

bool CompressedNetworkPeer::_decompressZlib(const char *data, size_t length, std::string &outData) {
    z_stream stream{};

    if (inflateInit2(&stream, RAW_DEFLATE_WINDOW_BITS) != Z_OK)
        return false;

    stream.next_in = (Bytef *) data;
    stream.avail_in = (uInt) length;

    outData.clear();

    char buffer[16384];
    int result;

    do {
        stream.next_out = (Bytef *) buffer;
        stream.avail_out = (uInt) sizeof(buffer);

        result = inflate(&stream, Z_NO_FLUSH);

        if (result != Z_OK && result != Z_STREAM_END && result != Z_BUF_ERROR) {
            inflateEnd(&stream);
            return false;
        }

        outData.append(buffer, sizeof(buffer) - stream.avail_out);

        if (outData.size() > MAX_DECOMPRESSED_SIZE) {
            inflateEnd(&stream);
            return false;
        }
    } while (result != Z_STREAM_END && stream.avail_in > 0);

    inflateEnd(&stream);
    return result == Z_STREAM_END;
}

bool CompressedNetworkPeer::_compressSnappy(const std::string &input, std::string &outData) {
    return snappy::Compress(input.data(), input.size(), &outData) > 0;
}

bool CompressedNetworkPeer::_decompressSnappy(const char *data, size_t length, std::string &outData) {
    size_t uncompressed = 0;
    if (!snappy::GetUncompressedLength(data, length, &uncompressed) || uncompressed > MAX_DECOMPRESSED_SIZE)
        return false;
    return snappy::Uncompress(data, length, &outData);
}

bool CompressedNetworkPeer::compress(CompressionAlgorithm algorithm, const std::string &input, std::string &outData) {
    if (algorithm == CompressionAlgorithm::ZLib)
        return _compressZlib(input, outData);
    if (algorithm == CompressionAlgorithm::Snappy)
        return _compressSnappy(input, outData);
    return false;
}

void CompressedNetworkPeer::sendSharedBatch(const SharedPacket &packet, Reliability reliability) {
    const std::string &batch = packet.getBatch();
    // same choice as sendPacket: below the threshold the batch goes out raw
    if (!mCompressionEnabled || batch.size() < mCompressionThreshold) {
        sendPacket(batch, reliability, Compressibility::Compressible);
        return;
    }

    const std::string &compressed = packet.getCompressed(mAlgorithm);
    if (compressed.empty()) {
        sendPacket(batch, reliability, Compressibility::Compressible);
        return;
    }

    std::string payload;
    payload.reserve(compressed.size() + 2);
    if (mPeer->usesGamePacketId())
        payload.push_back((char) GAME_PACKET_ID);
    payload.push_back((char) mAlgorithm);
    payload.append(compressed);
    mPeer->sendPacket(payload, reliability, Compressibility::Compressible);
}

void CompressedNetworkPeer::sendPacket(const std::string &data, Reliability reliability,
                                       Compressibility compressibility) {
    if (data.empty())
        return;

    const bool gamePacketId = mPeer->usesGamePacketId();

    std::string payload;
    if (gamePacketId)
        payload.push_back((char) GAME_PACKET_ID);

    if (!mCompressionEnabled) {
        payload.append(data);
        mPeer->sendPacket(payload, reliability, compressibility);
        return;
    }

    const bool shouldCompress = compressibility == Compressibility::Compressible &&
                                (mAlgorithm == CompressionAlgorithm::ZLib || mAlgorithm == CompressionAlgorithm::Snappy) &&
                                data.size() >= mCompressionThreshold;

    std::string compressed;
    const bool packed = shouldCompress &&
                        (mAlgorithm == CompressionAlgorithm::Snappy ? _compressSnappy(data, compressed)
                                                                    : _compressZlib(data, compressed));
    if (packed) {
        payload.push_back((char) mAlgorithm);
        payload.append(compressed);
    } else {
        payload.push_back((char) CompressionAlgorithm::None);
        payload.append(data);
    }

    mPeer->sendPacket(payload, reliability, compressibility);
}

NetworkPeer::DataStatus CompressedNetworkPeer::receivePacket(std::string &outData) {
    std::string payload;

    for (;;) {
        if (mPeer->receivePacket(payload) == DataStatus::NoData)
            return DataStatus::NoData;

        const bool gamePacketId = mPeer->usesGamePacketId();

        if (gamePacketId && (payload.size() < 2 || (unsigned char) payload[0] != GAME_PACKET_ID))
            continue;

        if (!gamePacketId && payload.empty())
            continue;

        const char *body = payload.data() + (gamePacketId ? 1 : 0);
        size_t bodyLength = payload.size() - (gamePacketId ? 1 : 0);

        if (!mCompressionEnabled) {
            outData.assign(body, bodyLength);
            return DataStatus::HasData;
        }

        const CompressionAlgorithm algorithm = (CompressionAlgorithm) (unsigned char) body[0];
        body += 1;
        bodyLength -= 1;

        if (algorithm == CompressionAlgorithm::None) {
            outData.assign(body, bodyLength);
            return DataStatus::HasData;
        }

        if (algorithm == CompressionAlgorithm::ZLib) {
            if (!_decompressZlib(body, bodyLength, outData))
                continue;
            _observe(algorithm, body, bodyLength, outData);
            return DataStatus::HasData;
        }

        if (algorithm == CompressionAlgorithm::Snappy) {
            if (!_decompressSnappy(body, bodyLength, outData))
                continue;
            _observe(algorithm, body, bodyLength, outData);
            return DataStatus::HasData;
        }

        continue;
    }
}

void CompressedNetworkPeer::_observe(CompressionAlgorithm algorithm, const char *body, size_t length,
                                     const std::string &batch) {
    if (mBatchTraits.mBatches >= OBSERVED_BATCHES)
        return;
    mBatchTraits.mBatches++;

    // the game sends a batch shorter than the threshold as it is
    if (batch.size() < mCompressionThreshold)
        mBatchTraits.mCompressedBelowThreshold++;
    if (algorithm != mAlgorithm)
        mBatchTraits.mOtherAlgorithm++;

    if (algorithm != CompressionAlgorithm::ZLib || batch.size() > MAX_COMPARED_SIZE || !deflatesLikeReference())
        return;
    std::string expected;
    if (!deflateLikeGame(batch.data(), batch.size(), expected))
        return;
    mBatchTraits.mCompared++;
    if (expected.size() != length || expected.compare(0, length, body, length) != 0)
        mBatchTraits.mNotReproduced++;
}

NetworkPeer::NetworkStatus CompressedNetworkPeer::getNetworkStatus() const {
    return mPeer->getNetworkStatus();
}

void CompressedNetworkPeer::update() {
    mPeer->update();
}

void CompressedNetworkPeer::flush() {
    mPeer->flush();
}
