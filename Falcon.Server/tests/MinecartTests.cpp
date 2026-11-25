#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Movement/PushableByEntity.h"
#include "World/Actor/Vehicle/MinecartActor.h"
#include "World/Item/MinecartItem.h"
#include "World/Level/Block/DispenseSource.h"
#include "World/Level/Block/Inventory/SimpleContainerInventory.h"
#include "Protocol/Types/ItemDefinition.h"
#include "World/Item/VanillaItems.h"
#include "World/Level/Block/BaseRailBlock.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    bool near(float a, float b) {
        return std::fabs(a - b) < 0.0001f;
    }

    BlockState rail(const char *name, int direction, bool powered = false) {
        Tag states = Tag::ofCompound();
        states.putInt("rail_direction", direction);
        if (std::string(name) != "minecraft:rail")
            states.putByte("rail_data_bit", powered ? 1 : 0);
        return BlockState(name, states);
    }

    struct Fixture {
        ServerNetworkHandler mOwner{"Minecarts", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Fixture() {
            mLevel.getChunk(0, 0);
            for (int x = 0; x < 16; ++x)
                for (int z = 0; z < 16; ++z)
                    mLevel.setBlockState(x, 63, z, BlockState("minecraft:stone"));
        }
    };

    void railGeometry() {
        Fixture fixture;
        fixture.mLevel.setBlockState(8, 64, 8, rail("minecraft:rail", BaseRailBlock::STRAIGHT_NORTH_SOUTH));
        const auto flat = MinecartActor::railPosition(fixture.mLevel, Vector3f(8.4f, 64.2f, 8.3f));
        require(flat.has_value() && near(flat->x, 8.5f) && near(flat->y, 64.0f) && near(flat->z, 8.3f),
                "a flat rail centres the minecart at the rail's height");

        fixture.mLevel.setBlockState(9, 64, 8, rail("minecraft:rail", BaseRailBlock::ASCENDING_NORTH));
        const auto slope = MinecartActor::railPosition(fixture.mLevel, Vector3f(9.5f, 64.0f, 8.5f));
        require(slope.has_value() && near(slope->y, 64.5f), "the middle of a slope is half a block up");
        require(!MinecartActor::railPosition(fixture.mLevel, Vector3f(2.5f, 64.0f, 2.5f)).has_value(),
                "nothing off a rail");
    }

    void itemsPlaceTheirMinecart() {
        require(MinecartActor::actorOfItem("minecraft:minecart") == "minecraft:minecart", "the minecart item");
        require(MinecartActor::actorOfItem("minecraft:hopper_minecart") == "minecraft:hopper_minecart",
                "the hopper minecart item");
        require(MinecartActor::actorOfItem("minecraft:rail").empty(), "a rail is no minecart");
        MinecartActor chest(1, "minecraft:chest_minecart");
        MinecartActor hopper(2, "minecraft:hopper_minecart");
        MinecartActor tnt(3, "minecraft:tnt_minecart");
        require(chest.getEntityContainer() != nullptr && chest.getEntityContainer()->getContainerSize() == 27
                        && hopper.getEntityContainer()->getContainerSize() == 5 && tnt.getEntityContainer() == nullptr,
                "the chest and hopper minecarts carry their slots");
    }

    void minecartsPushAsLegacyMinecarts() {
        const json::Value *component =
                EntityDefinitions::find("minecraft:minecart")->get("components")->get("minecraft:pushable_by_entity");
        PushableByEntityPresets presets;
        presets.refresh(component);
        require(!presets.mPresets.empty() && presets.mPresets.back().mMode == PushMode::LegacyMinecart,
                "the minecart pushes in legacy minecart mode");
    }

    void poweredRailStartsAMinecart() {
        Fixture fixture;
        // a stopped minecart on a powered rail starts away from the block beside it
        fixture.mLevel.setBlockState(4, 64, 8, BlockState("minecraft:stone"));
        for (int x = 5; x < 12; ++x)
            fixture.mLevel.setBlockState(x, 64, 8, rail("minecraft:golden_rail", BaseRailBlock::STRAIGHT_EAST_WEST, true));
        ServerActor *minecart = fixture.mOwner.spawnActor(fixture.mLevel, "minecraft:minecart",
                                                          Vector3f(5.5f, 64.0f, 8.5f));
        require(minecart != nullptr, "the minecart did not spawn");
        for (int tick = 0; tick < 10; ++tick)
            minecart->tickMovement(fixture.mOwner);
        require(minecart->getMotion().x > 0.0f && minecart->getPosition().x > 5.6f,
                "a powered rail did not start the minecart away from the stone");
        require(near(minecart->getPosition().z, 8.5f), "the minecart left the middle of its rail");
    }

    size_t minecartsIn(ServerNetworkHandler &owner) {
        size_t count = 0;
        for (auto &entry: owner.getActors())
            count += entry.second->isMinecart() && !entry.second->isExpired() ? 1 : 0;
        return count;
    }

    void dispenserPutsAMinecartOnTheRail() {
        Fixture fixture;
        fixture.mLevel.setBlockState(9, 64, 8, rail("minecraft:rail", BaseRailBlock::STRAIGHT_EAST_WEST));
        SimpleContainerInventory container(9);
        ItemStack minecart;
        minecart.mDefinition = std::make_shared<ItemDefinition>("minecraft:minecart", 1, false, Tag());
        minecart.mCount = 2;
        container.setContainerItem(0, minecart);
        const MinecartItem item(Item(*VanillaItems::fromIdentifier("minecraft:minecart")));
        // facing east, toward the rail
        const DispenseSource east{fixture.mOwner, fixture.mLevel, Vector3i(8, 64, 8), container, 0,
                                  Vector3f(9.2f, 64.3f, 8.5f), 5};
        require(item.dispense(east) && container.getContainerItem(0).mCount == 1
                        && minecartsIn(fixture.mOwner) == 1,
                "a dispenser facing a rail did not place a minecart");
        // facing west, toward stone: nothing to place on
        fixture.mLevel.setBlockState(7, 64, 8, BlockState("minecraft:stone"));
        const DispenseSource west{fixture.mOwner, fixture.mLevel, Vector3i(8, 64, 8), container, 0,
                                  Vector3f(7.8f, 64.3f, 8.5f), 4};
        require(!item.dispense(west) && container.getContainerItem(0).mCount == 1,
                "a dispenser without a rail in front placed a minecart");
    }

    void lavaLightsATntMinecart() {
        Fixture fixture;
        fixture.mLevel.setBlockState(8, 64, 8, BlockState("minecraft:lava"));
        ServerActor *tnt = fixture.mOwner.spawnActor(fixture.mLevel, "minecraft:tnt_minecart",
                                                     Vector3f(8.5f, 64.0f, 8.5f));
        require(tnt != nullptr, "the TNT minecart did not spawn");
        tnt->tick(fixture.mOwner);
        require(tnt->isExpired(), "a TNT minecart in lava did not go off");
    }

    void commandBlockMinecartShowsItsCommand() {
        Fixture fixture;
        auto *minecart = dynamic_cast<MinecartActor *>(fixture.mOwner.spawnActor(
                fixture.mLevel, "minecraft:command_block_minecart", Vector3f(8.5f, 64.0f, 8.5f)));
        require(minecart != nullptr, "the command block minecart did not spawn");
        minecart->setCommand(fixture.mOwner, "say hi", "", true);
        EntityDataMap metadata;
        minecart->fillSpawnMetadata(metadata);
        bool command = false;
        for (const EntityDataEntry &entry: metadata.mEntries)
            command = command || (entry.mId == 71 && entry.mStringValue == "say hi");
        require(command, "the command block minecart does not carry its command");
    }
}

int main() {
    try {
        EntityDefinitions::initialize();
        railGeometry();
        itemsPlaceTheirMinecart();
        minecartsPushAsLegacyMinecarts();
        poweredRailStartsAMinecart();
        dispenserPutsAMinecartOnTheRail();
        lavaLightsATntMinecart();
        commandBlockMinecartShowsItsCommand();
    } catch (const std::exception &error) {
        std::clog << "Minecart regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
