#pragma once

#include "Core/Math/Vector3i.h"
#include "Protocol/Packet.h"

#include <cstdint>
#include <string>

class GameTestRequestPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::GameTestRequest;

    GameTestRequestPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "GameTestRequestPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int32_t mMaxTestsPerBatch = 0;
    int32_t mRepeatCount = 0;
    int8_t mRotation = 0;
    bool mStoppingOnFailure = false;
    Vector3i mTestPos;
    int32_t mTestsPerRow = 0;
    std::string mTestName;
};
