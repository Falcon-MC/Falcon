#pragma once

#include "Core/Math/Vector3i.h"
#include "Protocol/Packet.h"

#include <cstdint>
#include <string>

enum class CommandBlockMode {
    Normal,
    Repeating,
    Chain,
};

class CommandBlockUpdatePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::CommandBlockUpdate;

    CommandBlockUpdatePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "CommandBlockUpdatePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    bool mBlock = false;
    Vector3i mBlockPosition;
    CommandBlockMode mMode = CommandBlockMode::Normal;
    bool mRedstoneMode = false;
    bool mConditional = false;
    uint64_t mMinecartRuntimeActorId = 0;
    std::string mCommand;
    std::string mLastOutput;
    std::string mName;
    std::string mFilteredName;
    bool mOutputTracked = false;
    uint32_t mTickDelay = 0;
    bool mExecutingOnFirstTick = false;
};
