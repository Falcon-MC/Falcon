#include "Protocol/Packets/LessonProgressPacket.h"

#include "Protocol/NetworkPacketHandler.h"

LessonProgressPacket::LessonProgressPacket() = default;

void LessonProgressPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarInt((int32_t) mAction);
    stream.putVarInt(mScore);
    stream.putString(mActivityId);
}

void LessonProgressPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mAction = (Action) stream.getVarInt();
    mScore = stream.getVarInt();
    mActivityId = stream.getString();
}

void LessonProgressPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
