#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/CommandOriginData.h"

#include <cstdint>
#include <string>
#include <vector>

enum class CommandOutputType : int32_t {
    None = 0,
    LastOutput = 1,
    Silent = 2,
    AllOutput = 3,
    DataSet = 4,
};

class CommandOutputMessage {
public:
    bool mInternal = false;
    std::string mMessageId;
    std::vector<std::string> mParameters;
};

class CommandOutputPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::CommandOutput;

    CommandOutputPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "CommandOutputPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    CommandOriginData mOrigin;
    CommandOutputType mType = CommandOutputType::AllOutput;
    int32_t mSuccessCount = 0;
    std::vector<CommandOutputMessage> mMessages;

    bool mHasData = false;
    std::string mData;
};
