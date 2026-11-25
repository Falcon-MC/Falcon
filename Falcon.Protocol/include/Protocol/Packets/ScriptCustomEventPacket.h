#pragma once

#include "Protocol/Packet.h"

#include <string>

class ScriptCustomEventPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ScriptCustomEvent;

    ScriptCustomEventPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ScriptCustomEventPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mEventName;
    std::string mData;
};
