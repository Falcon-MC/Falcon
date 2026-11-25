#include "Protocol/Packets/ClientboundDataStorePacket.h"

#include "Protocol/DataStoreCodec.h"
#include "Protocol/NetworkPacketHandler.h"

ClientboundDataStorePacket::ClientboundDataStorePacket() = default;

void ClientboundDataStorePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mUpdates.size());
    for (const DataStoreAction &action: mUpdates) {
        DataStoreCodec::writeAction(stream, action, true);
    }
}

void ClientboundDataStorePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    uint32_t length = stream.getArrayLength();
    mUpdates.reserve(length);
    for (uint32_t i = 0; i < length; i++) {
        mUpdates.push_back(DataStoreCodec::readAction(stream, true));
    }
}

void ClientboundDataStorePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
