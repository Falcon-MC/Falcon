#pragma once

#include "Core/Utility/UUID.h"
#include "Protocol/Packet.h"
#include "Protocol/Types/SerializedSkin.h"

#include <string>

class PlayerSkinPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::PlayerSkin;

    PlayerSkinPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "PlayerSkinPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Uuid mUuid;
    SerializedSkin mSkin;
    std::string mNewSkinName;
    std::string mOldSkinName;
    bool mTrustedSkin = false;
};
