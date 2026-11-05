#include "World/Actor/Mob/Component/MobAddRider.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/RideSystem.h"
#include "Network/ServerNetworkHandler.h"


namespace {
    std::string textOf(const json::Value &object, const char *key) {
        const json::Value *value = object.get(key);
        return value == nullptr ? std::string() : value->string();
    }
}

void MobAddRider::queue(const json::Value *component) {
    if (component != nullptr)
        mPending.push_back(component);
}

void MobAddRider::spawnPending(ServerNetworkHandler &owner, MobActor &vehicle) {
    if (mPending.empty())
        return;

    std::vector<const json::Value *> pending;
    pending.swap(mPending);
    for (const json::Value *component: pending) {
        // sub_146AA0F60: the riders list wins, the legacy entity_type only counts when the list is empty
        const json::Value *riders = component->get("riders");
        if (riders != nullptr && riders->isArray() && !riders->mArray.empty()) {
            for (const std::unique_ptr<json::Value> &rider: riders->mArray)
                _spawnRider(owner, vehicle, textOf(*rider, "entity_type"), textOf(*rider, "spawn_event"));
            continue;
        }

        const std::string identifier = textOf(*component, "entity_type");
        if (!identifier.empty())
            _spawnRider(owner, vehicle, identifier, textOf(*component, "spawn_event"));
    }
}

void MobAddRider::_spawnRider(ServerNetworkHandler &owner, MobActor &vehicle, const std::string &identifier,
                              const std::string &event) {
    // sub_14313C9A0: the rider is created at the vehicle with the spawn event as its init event, then starts riding,
    // and is removed again when the vehicle does not take it
    if (identifier.empty())
        return;

    const auto configure = [&event](ServerActor &actor) {
        MobActor *mob = dynamic_cast<MobActor *>(&actor);
        if (mob != nullptr && !event.empty())
            mob->setSpawnEvent(event);
    };
    MobActor *rider = dynamic_cast<MobActor *>(
            owner.spawnActor(owner.getLevelFor(vehicle), identifier, vehicle.getPosition(), configure));
    if (rider == nullptr)
        return;

    rider->startDefinition(owner);
    if (!vehicle.acceptsRider(*rider) || !RideSystem::mount(owner, *rider, vehicle, false))
        rider->despawn();
}
