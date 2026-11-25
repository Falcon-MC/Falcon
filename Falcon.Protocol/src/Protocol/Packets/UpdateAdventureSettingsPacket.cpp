#include "Protocol/Packets/UpdateAdventureSettingsPacket.h"

#include "Protocol/NetworkPacketHandler.h"

UpdateAdventureSettingsPacket::UpdateAdventureSettingsPacket() = default;

void UpdateAdventureSettingsPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putBool(mNoPvM);
    stream.putBool(mNoMvP);
    stream.putBool(mImmutableWorld);
    stream.putBool(mShowNameTags);
    stream.putBool(mAutoJump);
}

void UpdateAdventureSettingsPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mNoPvM = stream.getBool();
    mNoMvP = stream.getBool();
    mImmutableWorld = stream.getBool();
    mShowNameTags = stream.getBool();
    mAutoJump = stream.getBool();
}

void UpdateAdventureSettingsPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
