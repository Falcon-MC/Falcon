#include "Protocol/Packets/RequestNetworkSettingsPacket.h"

#include "Protocol/NetworkPacketHandler.h"

RequestNetworkSettingsPacket::RequestNetworkSettingsPacket()
        : mProtocolVersion(0) {
    mCompressible = Packet::Compressibility::Incompressible;
}

void RequestNetworkSettingsPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putInt((uint32_t) mProtocolVersion);
}

void RequestNetworkSettingsPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mProtocolVersion = stream.getSignedInt();
}

void RequestNetworkSettingsPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
