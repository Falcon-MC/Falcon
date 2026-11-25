#pragma once

#include "Protocol/Packet.h"

#include <string>

class GameTestResultsPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::GameTestResults;

    GameTestResultsPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "GameTestResultsPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    bool mSuccessful = false;
    std::string mError;
    std::string mTestName;
};
