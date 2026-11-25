#include "Protocol/Packets/PacketViolationWarningPacket.h"

#include "Protocol/NetworkPacketHandler.h"

PacketViolationWarningPacket::PacketViolationWarningPacket() = default;

void PacketViolationWarningPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarInt((int32_t) mType - 1);
    stream.putVarInt((int32_t) mSeverity - 1);
    stream.putVarInt(mPacketCauseId);
    stream.putString(mContext);
}

void PacketViolationWarningPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mType = (PacketViolationType) (stream.getVarInt() + 1);
    mSeverity = (PacketViolationSeverity) (stream.getVarInt() + 1);
    mPacketCauseId = stream.getVarInt();
    mContext = stream.getString();
}

void PacketViolationWarningPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
