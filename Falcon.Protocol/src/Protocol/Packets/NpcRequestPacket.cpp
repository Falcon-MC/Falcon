#include "Protocol/Packets/NpcRequestPacket.h"

#include "Protocol/NetworkPacketHandler.h"

NpcRequestPacket::NpcRequestPacket()
        : mRuntimeActorId(0), mRequestType(RequestType::SetAction), mActionType(0) {}

void NpcRequestPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarLong(mRuntimeActorId);
    stream.putByte((unsigned char) mRequestType);
    stream.putString(mCommand);
    stream.putByte((unsigned char) mActionType);
    stream.putString(mSceneName);
}

void NpcRequestPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mRuntimeActorId = stream.getUnsignedVarLong();
    mRequestType = (RequestType) stream.getByte();
    mCommand = stream.getString();
    mActionType = stream.getByte();
    mSceneName = stream.getString();
}

void NpcRequestPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
