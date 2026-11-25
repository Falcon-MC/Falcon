#include "Protocol/Packets/CommandRequestPacket.h"

#include "Protocol/NetworkPacketHandler.h"

namespace {
    const char *LATEST_VERSION_NAME = "latest";
}

CommandRequestPacket::CommandRequestPacket() = default;

void CommandRequestPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mCommand);
    mOrigin.write(stream);
    stream.putBool(mInternal);
    stream.putString(LATEST_VERSION_NAME);
}

void CommandRequestPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mCommand = stream.getString();
    mOrigin.read(stream);
    mInternal = stream.getBool();
    stream.getString();
    mVersion = 48;
}

void CommandRequestPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
