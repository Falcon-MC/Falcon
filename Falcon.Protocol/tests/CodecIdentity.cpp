#include "Protocol/Codec/ProtocolCodecRegistry.h"
#include "Protocol/Packet.h"
#include "Protocol/PacketCodecContext.h"

#include <iostream>
#include <memory>
#include <string>

namespace {

    bool encodeDirectly(const Packet &packet, const PacketCodecContext &context, std::string &outBytes) {
        try {
            BinaryStream stream;
            packet.writeHeader(stream);
            packet.write(stream, context);
            outBytes = stream.getBuffer();
            return true;
        } catch (const std::exception &) {
            return false;
        }
    }

    bool encodeThroughCodec(const Packet &packet, const PacketCodecContext &context, std::string &outBytes) {
        try {
            BinaryStream stream;
            packet.writeWithHeader(stream, context);
            outBytes = stream.getBuffer();
            return true;
        } catch (const std::exception &) {
            return false;
        }
    }

}

int main() {
    BlockDefinitionRegistry blocks;
    ItemDefinitionRegistry items;
    PacketCodecContext context(blocks, items);
    const ProtocolCodec &codec = ProtocolCodecRegistry::instance().getDefault();

    int compared = 0;
    int mismatches = 0;
    for (int id = 0; id < 1024; ++id) {
        const std::shared_ptr<Packet> packet = codec.createPacket((MinecraftPacketIds) id);
        if (packet == nullptr)
            continue;

        std::string direct;
        std::string throughCodec;
        const bool directWritten = encodeDirectly(*packet, context, direct);
        const bool codecWritten = encodeThroughCodec(*packet, context, throughCodec);
        if (directWritten != codecWritten || direct != throughCodec) {
            std::cerr << "packet " << id << " (" << packet->getName() << ") is encoded differently\n";
            ++mismatches;
        }
        ++compared;
    }

    std::cout << compared << " packets compared, " << mismatches << " mismatches\n";
    return compared > 0 && mismatches == 0 ? 0 : 1;
}
