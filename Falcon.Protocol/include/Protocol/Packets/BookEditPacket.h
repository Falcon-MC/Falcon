#pragma once

#include "Protocol/Packet.h"

#include <cstdint>
#include <string>

class BookEditPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::BookEdit;

    enum class Action {
        ReplacePage,
        AddPage,
        DeletePage,
        SwapPages,
        SignBook
    };

    BookEditPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "BookEditPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Action mAction = Action::ReplacePage;
    int32_t mInventorySlot = 0;
    int32_t mPageNumber = 0;
    int32_t mSecondaryPageNumber = 0;
    std::string mText;
    std::string mPhotoName;
    std::string mTitle;
    std::string mAuthor;
    std::string mXuid;
};
