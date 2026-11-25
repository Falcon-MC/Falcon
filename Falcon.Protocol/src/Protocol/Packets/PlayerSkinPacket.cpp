#include "Protocol/Packets/PlayerSkinPacket.h"

#include "Protocol/NetworkPacketHandler.h"
#include "Protocol/SkinCodec.h"

PlayerSkinPacket::PlayerSkinPacket() = default;

void PlayerSkinPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUuid(mUuid);
    const std::string noPlayFabId;
    SkinCodec::writeSkin(stream, mSkin, context.getCapabilities().mPlayerListPlayFabId ? nullptr : &noPlayFabId);
    stream.putString(mNewSkinName);
    stream.putString(mOldSkinName);
    stream.putBool(mTrustedSkin);
}

void PlayerSkinPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mUuid = stream.getUuid();
    std::string playFabId;
    mSkin = SkinCodec::readSkin(stream, context.getCapabilities().mPlayerListPlayFabId ? nullptr : &playFabId);
    mNewSkinName = stream.getString();
    mOldSkinName = stream.getString();

    if (!stream.feof()) {
        mTrustedSkin = stream.getBool();
    }
}

void PlayerSkinPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
