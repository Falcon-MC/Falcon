#include "Actor/RideSystem.h"

#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/SetActorDataPacket.h"
#include "Protocol/Packets/SetActorLinkPacket.h"

namespace {
    constexpr int32_t RIDER_SEAT_POSITION = 56;

    EntityLinkType linkTypeFor(const Actor &vehicle, int64_t passengerId) {
        const std::vector<int64_t> &passengers = vehicle.getPassengers();
        if (!passengers.empty() && passengers.front() == passengerId)
            return EntityLinkType::Rider;

        return EntityLinkType::Passenger;
    }

    EntityLinkData makeLink(int64_t vehicleId, int64_t passengerId, EntityLinkType type, bool riderInitiated) {
        EntityLinkData link;
        link.mFrom = vehicleId;
        link.mTo = passengerId;
        link.mType = type;
        link.mImmediate = true;
        link.mRiderInitiated = riderInitiated;
        link.mVehicleAngularVelocity = 0.0f;
        return link;
    }

    void sendToWatchers(ServerNetworkHandler &owner, const Actor &vehicle, const Actor &passenger,
                        const Packet &packet) {
        for (auto &entry: owner.getPlayers()) {
            ServerPlayer &player = entry.second;
            if (!player.isSpawned())
                continue;

            const bool watches = player.getUniqueId() == passenger.getUniqueId()
                                 || owner.canPlayerSeeActor(player, vehicle)
                                 || owner.canPlayerSeeActor(player, passenger);
            if (!watches)
                continue;

            owner.getNetworkHandler().send(player.getNetworkIdentifier(), packet, owner.getCodecContext());
        }
    }

    void sendSeatPosition(ServerNetworkHandler &owner, Actor &vehicle, Actor &passenger,
                          const Vector3f &seat) {
        SetActorDataPacket packet;
        packet.mRuntimeActorId = (int64_t) passenger.getRuntimeId();

        EntityDataEntry entry;
        entry.mId = RIDER_SEAT_POSITION;
        entry.mFormat = EntityDataFormat::Vector3f;
        entry.mVector3fValue = seat;
        packet.mMetadata.mEntries.push_back(entry);

        sendToWatchers(owner, vehicle, passenger, packet);
    }

    Vector3f seatOffsetOf(const Actor &vehicle) {
        const ServerActor *actor = dynamic_cast<const ServerActor *>(&vehicle);
        if (actor == nullptr)
            return Vector3f(0.0f, 0.0f, 0.0f);

        return actor->getSeatOffset();
    }
}

bool RideSystem::mount(ServerNetworkHandler &owner, Actor &rider, ServerActor &vehicle, bool riderInitiated) {
    if (rider.getUniqueId() == vehicle.getUniqueId())
        return false;

    for (Actor *above = resolve(owner, vehicle.getVehicleId()); above != nullptr;
         above = resolve(owner, above->getVehicleId())) {
        if (above->getUniqueId() == rider.getUniqueId())
            return false;
    }

    if (rider.isRiding())
        dismount(owner, rider, riderInitiated);

    vehicle._addPassenger(rider.getUniqueId());
    rider._setVehicleId(vehicle.getUniqueId());
    rider.getFlags().set(ActorFlag::Riding, true);

    const Vector3f seat = seatOffsetOf(vehicle);
    const Vector3f position = vehicle.getPosition();
    rider.setPosition(Vector3f(position.x + seat.x, position.y + seat.y, position.z + seat.z));

    SetActorLinkPacket packet;
    packet.mActorLink = makeLink(vehicle.getUniqueId(), rider.getUniqueId(),
                                 linkTypeFor(vehicle, rider.getUniqueId()), riderInitiated);
    sendToWatchers(owner, vehicle, rider, packet);
    sendSeatPosition(owner, vehicle, rider, seat);

    return true;
}

void RideSystem::dismount(ServerNetworkHandler &owner, Actor &rider, bool riderInitiated) {
    Actor *vehicle = resolve(owner, rider.getVehicleId());

    rider._setVehicleId(0);
    rider.getFlags().set(ActorFlag::Riding, false);

    if (vehicle == nullptr)
        return;

    vehicle->_removePassenger(rider.getUniqueId());

    SetActorLinkPacket packet;
    packet.mActorLink = makeLink(vehicle->getUniqueId(), rider.getUniqueId(), EntityLinkType::Remove,
                                 riderInitiated);
    sendToWatchers(owner, *vehicle, rider, packet);

    const Vector3f seat = seatOffsetOf(*vehicle);
    const Vector3f position = vehicle->getPosition();
    const Vector3f standing(position.x + seat.x, position.y + seat.y, position.z + seat.z);

    ServerPlayer *playerRider = dynamic_cast<ServerPlayer *>(&rider);
    if (playerRider != nullptr)
        playerRider->teleport(owner, standing);
    else
        rider.teleport(standing);
}

void RideSystem::ejectAll(ServerNetworkHandler &owner, Actor &vehicle) {
    const std::vector<int64_t> passengers = vehicle.getPassengers();
    for (const int64_t passengerId: passengers) {
        Actor *passenger = resolve(owner, passengerId);
        if (passenger != nullptr)
            dismount(owner, *passenger, false);
    }

    vehicle._clearPassengers();
}

void RideSystem::appendLinks(const Actor &actor, std::vector<EntityLinkData> &links) {
    for (const int64_t passengerId: actor.getPassengers())
        links.push_back(makeLink(actor.getUniqueId(), passengerId, linkTypeFor(actor, passengerId), false));
}

Actor *RideSystem::resolve(ServerNetworkHandler &owner, int64_t uniqueId) {
    if (uniqueId == 0)
        return nullptr;

    for (auto &entry: owner.getPlayers()) {
        if (entry.second.getUniqueId() == uniqueId)
            return &entry.second;
    }

    return owner.getActor(uniqueId);
}

void RideSystem::syncPassengerPositions(ServerNetworkHandler &owner, Actor &vehicle) {
    if (!vehicle.hasPassengers())
        return;

    const Vector3f seat = seatOffsetOf(vehicle);
    const Vector3f position = vehicle.getPosition();

    for (const int64_t passengerId: vehicle.getPassengers()) {
        Actor *passenger = resolve(owner, passengerId);
        if (passenger != nullptr)
            passenger->setPosition(Vector3f(position.x + seat.x, position.y + seat.y, position.z + seat.z));
    }
}
