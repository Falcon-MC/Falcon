#pragma once

#include "Protocol/MinecraftPacketIds.h"
#include "Protocol/Packet.h"

#include <functional>
#include <memory>

class ProtocolCodec;

class MinecraftPackets {
public:
    typedef std::function<std::shared_ptr<Packet>()> PacketFactory;

    /**
     * Creates a packet of the current protocol. A factory registered here replaces the one of the current
     * protocol's codec for that ID.
     */
    static std::shared_ptr<Packet> createPacket(MinecraftPacketIds id);

    static void registerPacket(MinecraftPacketIds id, const PacketFactory &factory);

    template<class PacketType>
    static void registerPacket() {
        registerPacket(PacketType::ID, []() { return std::static_pointer_cast<Packet>(
                std::make_shared<PacketType>()); });
    }

    /**
     * Registers every packet class of this library in the codec, each keeping its own read and write.
     */
    static void registerVanillaPackets(ProtocolCodec &codec);
};
