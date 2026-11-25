#include "Protocol/Packets/ClientCacheMissResponsePacket.h"

#include "Protocol/NetworkPacketHandler.h"

void ClientCacheMissResponsePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mBlobs.size());

    for (const std::pair<uint64_t, std::string> &blob: mBlobs) {
        stream.putLLong(blob.first);
        stream.putByteArray(blob.second);
    }
}

void ClientCacheMissResponsePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    const uint32_t count = stream.getArrayLength();

    mBlobs.clear();
    mBlobs.reserve(count);

    for (uint32_t i = 0; i < count; i++) {
        const uint64_t blobId = stream.getLLong();
        mBlobs.push_back(std::make_pair(blobId, stream.getByteArray()));
    }
}

void ClientCacheMissResponsePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
