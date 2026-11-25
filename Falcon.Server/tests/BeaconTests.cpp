#include "Network/ServerNetworkHandler.h"
#include "World/Actor/MobEffect.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/ItemStackRequestHandler.h"
#include "World/Level/Block/Actor/BeaconBlockActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Level.h"
#include "World/Level/WorldManager.h"

#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
    using Action = ItemStackRequestActionType;
    using Slot = ContainerSlotType;

    const Vector3i BEACON(8, 100, 8);

    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    ItemStack item(ServerNetworkHandler &server, const char *name) {
        ItemStack result = ItemStack::air();
        result.mDefinition = server.getItemDefinitions().getDefinition(name);
        require(result.mDefinition != nullptr, "missing item definition");
        result.mCount = 1;
        return result;
    }

    void buildPyramid(Level &level, int32_t levels, const char *block) {
        for (int32_t tier = 1; tier <= BeaconBlockActor::MAX_LEVELS; ++tier) {
            for (int32_t x = BEACON.x - tier; x <= BEACON.x + tier; ++x) {
                for (int32_t z = BEACON.z - tier; z <= BEACON.z + tier; ++z)
                    level.setBlock(Vector3i(x, BEACON.y - tier, z),
                                   BlockState(tier <= levels ? block : "minecraft:stone"), false);
            }
        }
    }

    void checkPyramid(Level &level) {
        int32_t levels = -1;
        for (int32_t expected = 0; expected <= BeaconBlockActor::MAX_LEVELS; ++expected) {
            buildPyramid(level, expected, "minecraft:iron_block");
            require(BeaconBlockActor::measurePyramid(level, BEACON, levels) && levels == expected,
                    "a full pyramid measured the wrong number of levels");
        }

        level.setBlock(Vector3i(BEACON.x + 2, BEACON.y - 2, BEACON.z - 2), BlockState("minecraft:gold_block"), false);
        level.setBlock(Vector3i(BEACON.x - 3, BEACON.y - 3, BEACON.z), BlockState("minecraft:netherite_block"), false);
        require(BeaconBlockActor::measurePyramid(level, BEACON, levels) && levels == 4,
                "mixed base blocks did not count as one pyramid");

        level.setBlock(Vector3i(BEACON.x + 2, BEACON.y - 2, BEACON.z + 1), BlockState("minecraft:air"), false);
        require(BeaconBlockActor::measurePyramid(level, BEACON, levels) && levels == 1,
                "a gap in the second layer kept the upper levels");
        buildPyramid(level, 4, "minecraft:diamond_block");
    }

    void checkBeam(Level &level) {
        const Vector3i above(BEACON.x, BEACON.y + 5, BEACON.z);
        require(BeaconBlockActor::measureBeam(level, BEACON), "an open sky blocked the beam");
        level.setBlock(above, BlockState("minecraft:glass"), false);
        require(BeaconBlockActor::measureBeam(level, BEACON), "glass blocked the beam");
        level.setBlock(above, BlockState("minecraft:bedrock"), false);
        require(BeaconBlockActor::measureBeam(level, BEACON), "bedrock blocked the beam");
        level.setBlock(above, BlockState("minecraft:stone"), false);
        require(!BeaconBlockActor::measureBeam(level, BEACON), "stone let the beam through");
        level.setBlock(above, BlockState("minecraft:air"), false);
    }

    void checkEffects() {
        const int32_t speed = (int32_t) MobEffectId::Speed;
        const int32_t strength = (int32_t) MobEffectId::Strength;
        const int32_t regeneration = (int32_t) MobEffectId::Regeneration;
        BeaconBlockActor::Effect effects[2];

        require(BeaconBlockActor::effectsFor(4, speed, regeneration, effects) == 2 && effects[0].mId == speed
                        && effects[0].mAmplifier == 0 && effects[1].mId == regeneration,
                "the top level did not give the primary and regeneration");
        require(BeaconBlockActor::effectsFor(4, speed, speed, effects) == 1 && effects[0].mAmplifier == 1,
                "a doubled primary did not become one effect a level higher");
        require(BeaconBlockActor::effectsFor(3, speed, regeneration, effects) == 1 && effects[0].mId == speed,
                "a three level pyramid gave a secondary");
        require(BeaconBlockActor::effectsFor(2, strength, 0, effects) == 0,
                "a two level pyramid gave strength");
        require(BeaconBlockActor::effectsFor(4, speed, strength, effects) == 1,
                "a secondary other than regeneration or the primary was given");

        require(BeaconBlockActor::durationFor(1) == 220 && BeaconBlockActor::durationFor(4) == 340,
                "wrong effect duration");
        require(BeaconBlockActor::rangeFor(1) == 21.0f && BeaconBlockActor::rangeFor(4) == 51.0f,
                "wrong range");
    }

    ItemStackRequest paymentRequest(int32_t primary, int32_t secondary, bool consume) {
        ItemStackRequest request;
        ItemStackRequestAction pay;
        pay.mType = Action::BeaconPayment;
        pay.mPrimaryEffect = primary;
        pay.mSecondaryEffect = secondary;
        request.mActions.push_back(pay);
        if (!consume)
            return request;

        ItemStackRequestAction take;
        take.mType = Action::Consume;
        take.mCount = 1;
        take.mSource.mContainer = Slot::BeaconPayment;
        take.mSource.mContainerName.mContainer = Slot::BeaconPayment;
        take.mSource.mSlot = PlayerInventory::BEACON_PAYMENT_NETWORK_SLOT;
        request.mActions.push_back(take);
        return request;
    }

    int execute(ServerNetworkHandler &server, PlayerInventory &inventory, const ItemStackRequest &request,
                int32_t levels, BeaconChoice &choice) {
        choice = BeaconChoice();
        return ItemStackRequestHandler::execute(
                       inventory, request, {}, {}, {}, false, false, nullptr, nullptr, &server.getCodecContext(),
                       nullptr, 0, false, nullptr, false, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, 0, false, nullptr, levels, &choice)
                .mResult;
    }

    void checkPayment(ServerNetworkHandler &server) {
        const int32_t speed = (int32_t) MobEffectId::Speed;
        const int32_t strength = (int32_t) MobEffectId::Strength;
        const int32_t regeneration = (int32_t) MobEffectId::Regeneration;
        PlayerInventory inventory;
        BeaconChoice choice;
        inventory.setBeaconPayment(item(server, "minecraft:iron_ingot"));

        require(execute(server, inventory, paymentRequest(speed, regeneration, true), -1, choice)
                        == ItemStackRequestHandler::RESULT_ERROR,
                "a payment without an open beacon was accepted");
        require(execute(server, inventory, paymentRequest(speed, 0, false), 4, choice)
                        == ItemStackRequestHandler::RESULT_ERROR,
                "a payment that took no item was accepted");
        require(execute(server, inventory, paymentRequest(strength, 0, true), 2, choice)
                        == ItemStackRequestHandler::RESULT_ERROR,
                "a two level pyramid sold strength");
        ItemStackRequest bare = paymentRequest(speed, 0, true);
        bare.mActions.erase(bare.mActions.begin());
        require(execute(server, inventory, bare, 4, choice) == ItemStackRequestHandler::RESULT_ERROR,
                "the payment slot emptied without a payment");
        require(!inventory.getBeaconPayment().isAir(), "a rejected payment took the item");

        require(execute(server, inventory, paymentRequest(speed, regeneration, true), 4, choice)
                        == ItemStackRequestHandler::RESULT_OK,
                "a valid payment failed");
        require(inventory.getBeaconPayment().isAir() && choice.mPaid && choice.mPrimary == speed
                        && choice.mSecondary == regeneration,
                "the payment did not take its item and report the choice");

        inventory.setBeaconPayment(item(server, "minecraft:dirt"));
        require(execute(server, inventory, paymentRequest(speed, 0, true), 4, choice)
                        == ItemStackRequestHandler::RESULT_ERROR,
                "dirt paid for a beacon");
    }

    void checkChoice() {
        BeaconBlockActor beacon;
        beacon.applyPayment((int32_t) MobEffectId::Strength, 0);
        require(beacon.getPrimaryEffect() == 0, "a beacon without a pyramid took strength");
    }

    void checkTick(ServerNetworkHandler &server, Level &level) {
        level.setBlock(BEACON, BlockState("minecraft:beacon"), false);
        auto created = std::make_unique<BeaconBlockActor>();
        created->setPosition(BEACON);
        level.getBlockActors().insert(std::move(created));
        BeaconBlockActor *beacon = level.getBlockActors().find<BeaconBlockActor>(BEACON);
        require(beacon != nullptr, "the beacon actor was not stored");

        const NetworkIdentifier id;
        ServerPlayer &player = server.getPlayers().try_emplace(id, id, 4002).first->second;
        player.setName("BeaconTest");
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.moveToLevel(level);
        player.setPosition(Vector3f(30.5f, 300.0f, 8.5f));

        for (int32_t tick = 0; tick < BeaconBlockActor::UPDATE_TICKS; ++tick)
            require(beacon->tick(server), "the beacon actor asked to be removed");
        require(beacon->getLevels() == 4 && beacon->hasBeam(), "the first update did not see the pyramid");

        beacon->applyPayment((int32_t) MobEffectId::Haste, (int32_t) MobEffectId::Haste);
        for (int32_t tick = 0; tick < BeaconBlockActor::UPDATE_TICKS; ++tick)
            beacon->tick(server);
        const MobEffectInstance *haste = player.getEffect(MobEffectId::Haste);
        require(haste != nullptr && haste->mAmplifier == 1 && haste->mDuration == 340 && haste->mAmbient,
                "a player above the beacon did not get haste II");

        player.removeEffect(MobEffectId::Haste);
        player.setPosition(Vector3f(8.5f, 40.0f, 8.5f));
        for (int32_t tick = 0; tick < BeaconBlockActor::UPDATE_TICKS; ++tick)
            beacon->tick(server);
        require(player.getEffect(MobEffectId::Haste) == nullptr, "a player below the range got haste");

        level.setBlock(BEACON, BlockState("minecraft:stone"), false);
        level.getBlockActors().tick(server);
        require(level.getBlockActors().find(BEACON) == nullptr, "a beacon actor outlived its block");
    }
}

int main() {
    try {
        ServerNetworkHandler server("Beacon", "", 1);
        Level &level = server.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        checkPyramid(level);
        checkBeam(level);
        checkEffects();
        checkPayment(server);
        checkChoice();
        checkTick(server, level);
    } catch (const std::exception &error) {
        std::clog << "Beacon regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
