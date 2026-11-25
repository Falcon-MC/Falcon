#pragma once

#include "Protocol/Packet.h"

#include <cstdint>
#include <string>

class LessonProgressPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::LessonProgress;

    enum class Action {
        Start,
        Complete,
        Restart
    };

    LessonProgressPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "LessonProgressPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Action mAction = Action::Start;
    int32_t mScore = 0;
    std::string mActivityId;
};
