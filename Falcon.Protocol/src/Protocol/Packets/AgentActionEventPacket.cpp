#include "Protocol/Packets/AgentActionEventPacket.h"

#include "Protocol/NetworkPacketHandler.h"

AgentActionEventPacket::AgentActionEventPacket() = default;

void AgentActionEventPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mRequestId);
    stream.putLInt((uint32_t) mActionType);
    stream.putString(mResponseJson);
}

void AgentActionEventPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mRequestId = stream.getString();
    mActionType = (AgentActionType) stream.getLInt();
    mResponseJson = stream.getString();
}

void AgentActionEventPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
