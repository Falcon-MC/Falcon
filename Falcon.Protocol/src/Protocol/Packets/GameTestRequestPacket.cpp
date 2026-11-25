#include "Protocol/Packets/GameTestRequestPacket.h"

#include "Protocol/NetworkPacketHandler.h"

GameTestRequestPacket::GameTestRequestPacket() = default;

void GameTestRequestPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarInt(mMaxTestsPerBatch);
    stream.putVarInt(mRepeatCount);
    stream.putByte((unsigned char) mRotation);
    stream.putBool(mStoppingOnFailure);
    stream.putVector3i(mTestPos);
    stream.putVarInt(mTestsPerRow);
    stream.putString(mTestName);
}

void GameTestRequestPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mMaxTestsPerBatch = stream.getVarInt();
    mRepeatCount = stream.getVarInt();
    mRotation = stream.getSignedByte();
    mStoppingOnFailure = stream.getBool();
    mTestPos = stream.getVector3i();
    mTestsPerRow = stream.getVarInt();
    mTestName = stream.getString();
}

void GameTestRequestPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
