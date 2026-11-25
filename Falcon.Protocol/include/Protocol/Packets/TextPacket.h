#pragma once

#include "Protocol/Packet.h"

#include <vector>

class TextPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::Text;

    enum class Type : int {
        Raw,
        Chat,
        Translation,
        Popup,
        JukeboxPopup,
        Tip,
        System,
        Whisper,
        Announcement,
        WhisperJson,
        Json,
        AnnouncementJson
    };

    TextPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "TextPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Type mType;
    std::string mSourceName;
    std::string mMessage;
    std::vector<std::string> mParameters;
    bool mNeedsTranslation;
    std::string mXuid;
    std::string mPlatformChatId;
    std::string mFilteredMessage;
};
