#include "World/Actor/RideSystem.h"

#include "World/Actor/ServerActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/Movement/PlayerMovementSimulator.h"
#include "Network/ServerNetworkHandler.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Protocol/Packets/SetActorDataPacket.h"
#include "Protocol/Packets/SetActorLinkPacket.h"

#include <cmath>

namespace {
    constexpr int32_t RIDER_SEAT_POSITION = 56;
    constexpr float DEGREES_TO_RADIANS = 0.017453292519943295f;

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

    void sendSeatPosition(ServerNetworkHandler &owner, Actor &vehicle, Actor &passenger, const Vector3f &seat) {
        SetActorDataPacket packet;
        packet.mRuntimeActorId = (int64_t) passenger.getRuntimeId();

        EntityDataEntry entry;
        entry.mId = RIDER_SEAT_POSITION;
        entry.mFormat = EntityDataFormat::Vector3f;
        entry.mVector3fValue = seat;
        packet.mMetadata.mEntries.push_back(entry);

        sendToWatchers(owner, vehicle, passenger, packet);
    }

    size_t passengerIndexOf(const Actor &vehicle, int64_t passengerId) {
        const std::vector<int64_t> &passengers = vehicle.getPassengers();
        for (size_t index = 0; index < passengers.size(); ++index) {
            if (passengers[index] == passengerId)
                return index;
        }
        return passengers.size();
    }

    Vector3f seatOffsetOf(const Actor &vehicle, size_t index, size_t passengerCount) {
        const ServerActor *actor = dynamic_cast<const ServerActor *>(&vehicle);
        if (actor == nullptr)
            return Vector3f(0.0f, 0.0f, 0.0f);

        return actor->getSeatOffset(index, passengerCount);
    }

    // vanilla writes the passenger's height offset into the synced seat: the client puts the passenger's network
    // position (eyes for a player) there; our positions are feet based: the server side keeps the bare seat
    float heightOffsetOf(const Actor &passenger) {
        if (dynamic_cast<const ServerPlayer *>(&passenger) != nullptr)
            return PlayerMovementSimulator::PLAYER_BASE_OFFSET;

        const ServerActor *actor = dynamic_cast<const ServerActor *>(&passenger);
        return actor == nullptr ? 0.0f : actor->getBaseOffset();
    }

    Vector3f seatPositionOf(const Actor &vehicle, const Vector3f &seat) {
        const float yaw = vehicle.getRotation().y * DEGREES_TO_RADIANS;
        const float cosine = std::cos(yaw);
        const float sine = std::sin(yaw);
        const Vector3f position = vehicle.getPosition();
        return Vector3f(position.x + seat.x * cosine - seat.z * sine, position.y + seat.y,
                        position.z + seat.x * sine + seat.z * cosine);
    }

    void refreshSeats(ServerNetworkHandler &owner, Actor &vehicle) {
        const std::vector<int64_t> passengers = vehicle.getPassengers();
        for (size_t index = 0; index < passengers.size(); ++index) {
            Actor *passenger = RideSystem::resolve(owner, passengers[index]);
            if (passenger == nullptr)
                continue;

            sendSeatPosition(owner, vehicle, *passenger,
                             RideSystem::syncedSeatOf(vehicle, *passenger, index, passengers.size()));
        }
    }
}

Vector3f RideSystem::syncedSeatOf(const Actor &vehicle, const Actor &passenger, size_t index, size_t passengerCount) {
    Vector3f seat = seatOffsetOf(vehicle, index, passengerCount);
    seat.y += heightOffsetOf(passenger);
    return seat;
}

bool RideSystem::mount(ServerNetworkHandler &owner, Actor &rider, ServerActor &vehicle, bool riderInitiated) {
    if (rider.getUniqueId() == vehicle.getUniqueId())
        return false;

    for (Actor *above = resolve(owner, vehicle.getVehicleId()); above != nullptr;
         above = resolve(owner, above->getVehicleId())) {
        if (above->getUniqueId() == rider.getUniqueId())
            return false;
    }

    if (PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_ENTITY_MOUNT)) {
        PluginEvent event;
        event.mType = FALCON_EVENT_ENTITY_MOUNT;
        event.mCancellable = true;
        event.mEntity = &rider;
        event.mTarget = &vehicle;
        event.mPlayer = dynamic_cast<ServerPlayer *>(&rider);
        plugins->dispatch(event);
        if (event.mCancelled)
            return false;
    }

    if (rider.isRiding())
        dismount(owner, rider, riderInitiated);

    vehicle._addPassenger(rider.getUniqueId());
    rider._setVehicleId(vehicle.getUniqueId());
    rider.getFlags().set(ActorFlag::Riding, true);

    const size_t count = vehicle.getPassengers().size();
    rider.setPosition(seatPositionOf(vehicle, seatOffsetOf(vehicle, count - 1, count)));

    SetActorLinkPacket packet;
    packet.mActorLink = makeLink(vehicle.getUniqueId(), rider.getUniqueId(), linkTypeFor(vehicle, rider.getUniqueId()),
                                 riderInitiated);
    sendToWatchers(owner, vehicle, rider, packet);
    refreshSeats(owner, vehicle);

    vehicle.onPassengerAdded(owner, rider);
    return true;
}

void RideSystem::dismount(ServerNetworkHandler &owner, Actor &rider, bool riderInitiated) {
    Actor *vehicle = resolve(owner, rider.getVehicleId());
    PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_ENTITY_DISMOUNT);
    if (plugins != nullptr && vehicle != nullptr) {
        PluginEvent event;
        event.mType = FALCON_EVENT_ENTITY_DISMOUNT;
        event.mEntity = &rider;
        event.mTarget = vehicle;
        event.mPlayer = dynamic_cast<ServerPlayer *>(&rider);
        plugins->dispatch(event);
        // a handler may have mounted the rider elsewhere or removed the vehicle
        vehicle = resolve(owner, rider.getVehicleId());
    }

    rider._setVehicleId(0);
    rider.getFlags().set(ActorFlag::Riding, false);

    if (vehicle == nullptr)
        return;

    const size_t index = passengerIndexOf(*vehicle, rider.getUniqueId());
    const size_t count = vehicle->getPassengers().size();
    ServerActor *serverVehicle = dynamic_cast<ServerActor *>(vehicle);
    const Vector3f standing = serverVehicle != nullptr ? serverVehicle->getDismountPosition(index, count)
                                                       : seatPositionOf(*vehicle, seatOffsetOf(*vehicle, index, count));

    vehicle->_removePassenger(rider.getUniqueId());

    SetActorLinkPacket packet;
    packet.mActorLink = makeLink(vehicle->getUniqueId(), rider.getUniqueId(), EntityLinkType::Remove, riderInitiated);
    sendToWatchers(owner, *vehicle, rider, packet);
    refreshSeats(owner, *vehicle);

    ServerPlayer *playerRider = dynamic_cast<ServerPlayer *>(&rider);
    if (playerRider == nullptr) {
        rider.teleport(standing);
    } else if (riderInitiated && !owner.getProperties().getServerAuthoritativeDismountStrict()
               && !vehicle->getFlags().get(ActorFlag::DoesServerAuthOnlyDismount)) {
        playerRider->getMovementValidator().allowDismountPosition();
    } else {
        playerRider->teleport(owner, standing);
    }

    if (serverVehicle != nullptr)
        serverVehicle->onPassengerRemoved(owner, rider);
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

    const std::vector<int64_t> &passengers = vehicle.getPassengers();

    for (size_t index = 0; index < passengers.size(); ++index) {
        Actor *passenger = resolve(owner, passengers[index]);
        if (passenger != nullptr)
            passenger->setPosition(seatPositionOf(vehicle, seatOffsetOf(vehicle, index, passengers.size())));
    }
}
