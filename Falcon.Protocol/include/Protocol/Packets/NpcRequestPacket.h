#pragma once

#include "Protocol/Packet.h"

class NpcRequestPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::NpcRequest;

    enum class RequestType : int {
        SetAction,
        ExecuteCommandAction,
        ExecuteClosingCommands,
        SetName,
        SetSkin,
        SetInteractionText,
        ExecuteOpeningCommands
    };

    NpcRequestPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "NpcRequestPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint64_t mRuntimeActorId;
    RequestType mRequestType;
    std::string mCommand;
    int32_t mActionType;
    std::string mSceneName;
};
