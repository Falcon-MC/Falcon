#pragma once

class NetworkIdentifier;
class Packet;

class PacketSender {
public:
    virtual ~PacketSender() = default;

    virtual void sendPacketTo(const NetworkIdentifier &id, const Packet &packet) = 0;
};
