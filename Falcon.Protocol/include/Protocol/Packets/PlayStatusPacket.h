#pragma once

#include "Protocol/Packet.h"

class PlayStatusPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::PlayStatus;

    enum class Status : int {
        LoginSuccess,
        LoginFailedClientOld,
        LoginFailedServerOld,
        PlayerSpawn,
        LoginFailedInvalidTenant,
        LoginFailedEditionMismatchEduToVanilla,
        LoginFailedEditionMismatchVanillaToEdu,
        FailedServerFullSubClient,
        EditorToVanillaMismatch,
        VanillaToEditorMismatch
    };

    PlayStatusPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "PlayStatusPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Status mStatus;
};
