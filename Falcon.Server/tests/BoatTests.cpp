#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Movement/PushableByEntity.h"
#include "World/Actor/Vehicle/BoatActor.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    bool near(float a, float b) {
        return a - b < 0.0001f && b - a < 0.0001f;
    }

    void variantsFollowTheItems() {
        require(BoatActor::variantOfItem("minecraft:oak_boat") == 0, "oak is variant 0");
        require(BoatActor::variantOfItem("minecraft:spruce_chest_boat") == 1, "spruce is variant 1");
        require(BoatActor::variantOfItem("minecraft:bamboo_raft") == BoatActor::RAFT_VARIANT, "the raft is variant 7");
        require(BoatActor::variantOfItem("minecraft:poplar_boat") == 10, "poplar is variant 10");
        require(BoatActor::variantOfItem("minecraft:boat") == 0, "the old boat item is oak");
        require(BoatActor::variantOfItem("minecraft:oak_planks") < 0, "planks are no boat");
        require(BoatActor::itemOf(5, false) == "minecraft:dark_oak_boat", "a dark oak boat drops its item");
        require(BoatActor::itemOf(BoatActor::RAFT_VARIANT, true) == "minecraft:bamboo_chest_raft",
                "a chest raft drops its item");
    }

    void seatsFollowTheDefinition() {
        BoatActor boat(1, BoatActor::BOAT);
        const Vector3f alone = boat.getSeatOffset(0, 1);
        require(near(alone.x, 0.0f) && near(alone.y, -0.2f), "a lone rider sits in the middle");
        const Vector3f back = boat.getSeatOffset(1, 2);
        require(near(back.x, -0.6f) && near(back.y, -0.2f), "the second rider sits at the back");

        BoatActor raft(2, BoatActor::BOAT);
        raft.setVariant(BoatActor::RAFT_VARIANT);
        require(near(raft.getSeatOffset(0, 1).y, 0.1f), "a raft seats its rider higher");

        BoatActor chest(3, BoatActor::CHEST_BOAT);
        require(chest.isChestBoat() && chest.getEntityContainer() != nullptr, "a chest boat carries a chest");
        require(near(chest.getSeatOffset(0, 1).x, 0.2f), "a chest boat seats its rider in front of the chest");
    }

    void boatsPushAlongTheirCentres() {
        BoatActor boat(4, BoatActor::BOAT);
        const json::Value *definition = EntityDefinitions::find(BoatActor::BOAT);
        const json::Value *component = definition->get("components")->get("minecraft:pushable_by_entity");
        PushableByEntityPresets presets;
        presets.refresh(component);
        require(presets.mPresets.size() == 2 && presets.mPresets[1].mMode == PushMode::LegacyBoat,
                "the boat's second preset is the legacy boat push");
        const PushableByEntityPreset &preset = presets.mPresets[1];
        require(near(preset.mMinDistance, 0.55f) && preset.mMaxDistance > 1000.0f, "the legacy push reaches far");

        // a pusher one block east: 0.1 strength, half of it for the boat, pushed west
        const Vector3f push = PushableByEntity::legacyBoatPush(preset, 1.0f, 0.0f);
        require(near(push.x, -0.05f) && near(push.z, 0.0f), "a boat is pushed away from its pusher");
        // closer than a block the strength stays whole
        const Vector3f close = PushableByEntity::legacyBoatPush(preset, 0.0f, 0.6f);
        require(near(close.z, -0.05f), "a close pusher pushes at full strength");
        require(!PushableByEntity::inReach(preset, 0.5f, 0.0f, 1.4f), "boats on top of each other do not push");

        require(boat.getLeashableComponent() != nullptr && boat.getActorLeash() != nullptr, "a boat takes a lead");
    }
}

int main() {
    try {
        EntityDefinitions::initialize();
        variantsFollowTheItems();
        seatsFollowTheDefinition();
        boatsPushAlongTheirCentres();
    } catch (const std::exception &error) {
        std::clog << "Boat regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
