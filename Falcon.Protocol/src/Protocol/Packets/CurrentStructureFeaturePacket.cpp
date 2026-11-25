#include "Protocol/Packets/CurrentStructureFeaturePacket.h"

#include "Protocol/NetworkPacketHandler.h"

CurrentStructureFeaturePacket::CurrentStructureFeaturePacket() = default;

void CurrentStructureFeaturePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mCurrentStructureFeature);
}

void CurrentStructureFeaturePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mCurrentStructureFeature = stream.getString();
}

void CurrentStructureFeaturePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
