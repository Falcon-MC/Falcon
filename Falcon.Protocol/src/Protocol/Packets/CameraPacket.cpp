#include "Protocol/Packets/CameraPacket.h"

#include "Protocol/NetworkPacketHandler.h"

CameraPacket::CameraPacket() = default;

void CameraPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarLong(mCameraUniqueActorId);
    stream.putVarLong(mPlayerUniqueActorId);
}

void CameraPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mCameraUniqueActorId = stream.getVarLong();
    mPlayerUniqueActorId = stream.getVarLong();
}

void CameraPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
