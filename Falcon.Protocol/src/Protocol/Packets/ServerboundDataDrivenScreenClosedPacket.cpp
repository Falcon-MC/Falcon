#include "Protocol/Packets/ServerboundDataDrivenScreenClosedPacket.h"

#include "Protocol/NetworkPacketHandler.h"

#include <array>

namespace {
    const std::array<std::string, 5> CLOSE_REASONS = {
        "programmaticclose", "programmaticcloseall", "clientcanceled", "userbusy", "invalidform"
    };
}

ServerboundDataDrivenScreenClosedPacket::ServerboundDataDrivenScreenClosedPacket() = default;

void ServerboundDataDrivenScreenClosedPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putLInt((uint32_t) mFormId);
    stream.putString(CLOSE_REASONS[(size_t) mCloseReason]);
}

void ServerboundDataDrivenScreenClosedPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mFormId = (int32_t) stream.getLInt();

    std::string reason = stream.getString();
    for (size_t i = 0; i < CLOSE_REASONS.size(); i++) {
        if (CLOSE_REASONS[i] == reason) {
            mCloseReason = (CloseReason) i;
            break;
        }
    }
}

void ServerboundDataDrivenScreenClosedPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
