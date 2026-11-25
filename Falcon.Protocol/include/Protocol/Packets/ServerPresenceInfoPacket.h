#pragma once

#include "Protocol/Packet.h"

#include <string>

class ServerPresenceInfoPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ServerPresenceInfo;

    ServerPresenceInfoPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ServerPresenceInfoPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    bool mHasPresenceConfiguration = false;
    bool mHasExperienceName = false;
    std::string mExperienceName;
    bool mHasWorldName = false;
    std::string mWorldName;
    std::string mRichPresenceId;
};
