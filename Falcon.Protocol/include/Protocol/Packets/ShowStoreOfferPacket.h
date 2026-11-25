#pragma once

#include "Protocol/Packet.h"

#include <string>

enum class StoreOfferRedirectType : unsigned char {
    Marketplace = 0,
    DressingRoom = 1,
    ThirdPartyServerPage = 2
};

class ShowStoreOfferPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ShowStoreOffer;

    ShowStoreOfferPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ShowStoreOfferPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mOfferId;
    StoreOfferRedirectType mRedirectType = StoreOfferRedirectType::Marketplace;
};
