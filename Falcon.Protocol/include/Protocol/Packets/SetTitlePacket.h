#pragma once

#include "Protocol/Packet.h"

class SetTitlePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::SetTitle;

    enum class Type : int {
        Clear,
        Reset,
        Title,
        Subtitle,
        Actionbar,
        Times,
        TitleJson,
        SubtitleJson,
        ActionbarJson
    };

    SetTitlePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "SetTitlePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Type mType;
    std::string mText;
    int32_t mFadeInTime;
    int32_t mStayTime;
    int32_t mFadeOutTime;
    std::string mXuid;
    std::string mPlatformOnlineId;
    std::string mFilteredTitleText;
};
