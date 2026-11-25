#include "Protocol/Packets/ServerboundDataStorePacket.h"

#include "Protocol/DataStoreCodec.h"
#include "Protocol/NetworkPacketHandler.h"

ServerboundDataStorePacket::ServerboundDataStorePacket() = default;

void ServerboundDataStorePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    DataStoreCodec::writeUpdate(stream, mUpdate, context.getCapabilities().mDataStoreVariables);
}

void ServerboundDataStorePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mUpdate = DataStoreCodec::readUpdate(stream, context.getCapabilities().mDataStoreVariables);
}

void ServerboundDataStorePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
