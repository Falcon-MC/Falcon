#include "Protocol/Packets/PlayerToggleArmorVisibilityRequestPacket.h"

#include "Protocol/NetworkPacketHandler.h"

PlayerToggleArmorVisibilityRequestPacket::PlayerToggleArmorVisibilityRequestPacket() = default;

void PlayerToggleArmorVisibilityRequestPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putBool(mHideArmor);
}

void PlayerToggleArmorVisibilityRequestPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mHideArmor = stream.getBool();
}

void PlayerToggleArmorVisibilityRequestPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
