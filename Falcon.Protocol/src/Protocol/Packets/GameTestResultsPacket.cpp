#include "Protocol/Packets/GameTestResultsPacket.h"

#include "Protocol/NetworkPacketHandler.h"

GameTestResultsPacket::GameTestResultsPacket() = default;

void GameTestResultsPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putBool(mSuccessful);
    stream.putString(mError);
    stream.putString(mTestName);
}

void GameTestResultsPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mSuccessful = stream.getBool();
    mError = stream.getString();
    mTestName = stream.getString();
}

void GameTestResultsPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
