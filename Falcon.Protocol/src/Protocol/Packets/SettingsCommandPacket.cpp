#include "Protocol/Packets/SettingsCommandPacket.h"

#include "Protocol/NetworkPacketHandler.h"

SettingsCommandPacket::SettingsCommandPacket()
        : mSuppressingOutput(false) {}

void SettingsCommandPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mCommand);
    stream.putBool(mSuppressingOutput);
}

void SettingsCommandPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mCommand = stream.getString();
    mSuppressingOutput = stream.getBool();
}

void SettingsCommandPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
