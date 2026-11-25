#pragma once

#include "Protocol/Packet.h"

#include <cstdint>
#include <string>
#include <vector>

class UnlockedRecipesPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::UnlockedRecipes;

    enum class ActionType : int32_t {
        Empty = 0,
        InitiallyUnlocked = 1,
        NewlyUnlocked = 2,
        RemoveUnlocked = 3,
        RemoveAll = 4
    };

    UnlockedRecipesPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "UnlockedRecipesPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    ActionType mAction = ActionType::Empty;
    std::vector<std::string> mUnlockedRecipes;
};
