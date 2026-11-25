#include "Protocol/Packets/ClientCacheBlobStatusPacket.h"

#include "Protocol/NetworkPacketHandler.h"

namespace {

    void writeBlobIds(BinaryStream &stream, const std::vector<uint64_t> &blobIds) {
        stream.putArrayLength((uint32_t) blobIds.size());

        for (uint64_t blobId: blobIds)
            stream.putLLong(blobId);
    }

    void readBlobIds(ReadOnlyBinaryStream &stream, std::vector<uint64_t> &blobIds) {
        const uint32_t count = stream.getUnsignedVarInt();
        if (count > ClientCacheBlobStatusPacket::MAX_BLOBS) {
            throw BinaryDataException("Tried to read " + std::to_string(count) + " blob ids but the maximum is " +
                                      std::to_string(ClientCacheBlobStatusPacket::MAX_BLOBS));
        }

        blobIds.clear();
        blobIds.reserve(count);

        for (uint32_t i = 0; i < count; i++)
            blobIds.push_back(stream.getLLong());
    }

}

void ClientCacheBlobStatusPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    writeBlobIds(stream, mNaks);
    writeBlobIds(stream, mAcks);
}

void ClientCacheBlobStatusPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    readBlobIds(stream, mNaks);
    readBlobIds(stream, mAcks);
}

void ClientCacheBlobStatusPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
