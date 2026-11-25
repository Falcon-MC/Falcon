#pragma once

#include "Protocol/Packet.h"

#include <cstdint>
#include <string>

class NpcDialoguePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::NpcDialogue;

    enum class Action {
        Open,
        Close
    };

    NpcDialoguePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "NpcDialoguePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mUniqueActorId = 0;
    Action mAction = Action::Open;
    std::string mDialogue;
    std::string mSceneName;
    std::string mNpcName;
    std::string mActionJson;
};
