#include "World/Actor/Movement/ServerPlayerMovementComponent.h"
#include "World/Actor/Movement/PlayerMovementSimulator.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/WaterBlock.h"
#include "World/Level/Block/LavaBlock.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Item/ElytraItem.h"
#include "Protocol/Packets/PlayerAuthInputPacket.h"
#include "Protocol/Types/StartGameTypes.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

class MovementInputProbe {
public:
    static void receive(ServerNetworkHandler &owner, const NetworkIdentifier &id, const PlayerAuthInputPacket &packet) {
        owner.handle(id, packet);
    }

    static void step(ServerNetworkHandler &owner, ServerPlayer &player, int64_t tick, double milliseconds = 0) {
        owner.mCurrentTick = tick;
        owner.mTickDurationSamples.clear();
        owner.mTickDurationSamples.push_back(milliseconds);
        owner._tickPlayer(player);
    }
};

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    PlayerAuthInputPacket input(int64_t tick, float x = 0.5f) {
        PlayerAuthInputPacket packet;
        packet.mTick = tick;
        packet.mPosition = Vector3f(x, 251.62f, 0.5f);
        return packet;
    }

    void checkTimeline() {
        PropertiesSettings defaults;
        require(defaults.getSimulation() && !defaults.getServerAuthoritativeMovementStrict(),
                "Simulation must be enabled independently of strict mode by default");
        require(PropertiesSettings::getDefaultContents().find("simulation=true\n") != std::string::npos,
                "Generated server.properties omitted the simulation switch");
        require(defaults.getPlayerTickThrottledInputBatchSize() == -1
                && defaults.getPlayerTickThrottledMaxTickCredits() == -1, "BDS tick policy defaults differ");
        ServerPlayerMovementComponent timeline;
        auto packet = input(100);
        packet.mInputData = {(int32_t) PlayerAuthInputData::StartJumping};
        require(timeline.enqueue(packet), "First input rejected");
        packet.mInputData.clear();
        require(!timeline.enqueue(input(100)) && !timeline.enqueue(input(99)) && !timeline.enqueue(input(-1)),
                "Duplicate, old or negative tick entered the timeline");
        timeline.beginTick(1, -1, -1);
        const auto stored = timeline.takeNext();
        require(stored != nullptr && stored->mTick == 100
                && stored->hasInputFlag((int32_t) PlayerAuthInputData::StartJumping), "Input was not owned by the queue");
        require(timeline.credits() == 0 && timeline.lastProcessedTick() == 100, "Input did not consume one credit");
        timeline.beginTick(1, -1, -1);
        require(timeline.credits() == 0, "Same server tick minted credits twice");
        timeline.beginTick(0, -1, -1);
        require(timeline.credits() == 0, "Backward server tick minted credits");
        require(timeline.enqueue(input(std::numeric_limits<int64_t>::max())), "Client tick origin was constrained");
        require(timeline.takeNext() == nullptr && timeline.credits() == 0, "A client tick jump minted credits");
        timeline.beginTick(2, -1, -1);
        require(timeline.takeNext() != nullptr, "Next server tick did not replenish a credit");

        ServerPlayerMovementComponent burst;
        burst.beginTick(1, -1, -1);
        for (int64_t tick = 1; tick <= 4; ++tick)
            require(burst.enqueue(input(tick)), "Burst enqueue failed");
        require(burst.takeNext()->mTick == 3, "BDS policy did not discard inputs beyond credits + 1");
        require(burst.takeNext() == nullptr && burst.pendingCount() == 1, "Uncredited tail was not deferred");
        burst.beginTick(2, -1, -1);
        require(burst.takeNext()->mTick == 4, "Deferred tail was lost");
        burst.beginTick(102, 5, 2);
        require(burst.credits() == 5, "Configured credit cap was ignored");
        for (int64_t tick = 5; tick <= 8; ++tick)
            burst.enqueue(input(tick));
        require(burst.takeNext()->mTick == 7 && burst.takeNext()->mTick == 8,
                "Configured batch policy did not retain the newest inputs");
        require(burst.credits() == 3, "Discarded inputs consumed credits");
        burst.enqueue(input(9));
        burst.clearPending();
        require(burst.pendingCount() == 0 && !burst.enqueue(input(9)) && burst.enqueue(input(10)),
                "Clearing pending coordinates reopened old client ticks");

        ServerPlayerMovementComponent full;
        for (size_t i = 0; i < ServerPlayerMovementComponent::MAX_PENDING_INPUTS; ++i)
            require(full.enqueue(input((int64_t) i)), "Queue filled before its bound");
        require(!full.enqueue(input(1000)) && full.pendingCount() == 1000, "Queue exceeded the BDS bound");
        full.beginTick(1, 0, -1);
        require(full.takeNext() == nullptr && full.pendingCount() == 1, "Zero-credit policy differs");
        full.beginTick(2, 1, -1);
        require(full.takeNext()->mTick == 999, "Latest deferred input was not retained");
        require(full.enqueue(input(1000)), "Full-queue rejection advanced the accepted tick");
        full.beginTick(3, 1, 0);
        require(full.takeNext() == nullptr && full.pendingCount() == 0, "Zero-sized batch policy differs");
        require(ServerPlayerMovementComponent::catchupSteps(0) == 1
                && ServerPlayerMovementComponent::catchupSteps(49.9) == 1
                && ServerPlayerMovementComponent::catchupSteps(50) == 3
                && ServerPlayerMovementComponent::catchupSteps(100) == 5
                && ServerPlayerMovementComponent::catchupSteps(1000000) == 41,
                "BDS catchup budget differs");
        require(ServerPlayerMovementComponent::catchupSteps(std::numeric_limits<double>::infinity()) == 1,
                "Nonfinite timing produced an unbounded catchup budget");
    }

    void checkPositionReconciliation() {
        const auto close = [](float actual, float expected, const char *message) {
            require(std::fabs(actual - expected) < 0.000001f, message);
        };
        for (bool strict: {false, true}) {
            PlayerMovementValidator validator;
            Vector3f position(0, 0, 0);
            require(!validator.reconcile(position, Vector3f(1, 0, 0), 5, 0.5f, strict),
                    "Correction ignored the initial BDS delay");
            require(validator.reconcile(position, Vector3f(1, 0, 0), 6, 0.5f, strict),
                    "Large position error was not corrected in both modes");
            require(!validator.reconcile(position, Vector3f(1, 0, 0), 11, 0.5f, strict)
                    && validator.reconcile(position, Vector3f(1, 0, 0), 12, 0.5f, strict),
                    "Corrections did not respect the strict greater-than-five tick gate");
            validator.reset();
            require(!validator.reconcile(position, Vector3f(0.004f, 0, 0), 100, 0.5f, strict),
                    "Strict mode rejected a position within BDS fine tolerance");
            close(position.x, 0.004f, "Fine tolerance did not adopt the client position");
        }

        PlayerMovementValidator normal;
        PlayerMovementValidator strict;
        Vector3f gradual(0, 0, 0);
        Vector3f authoritative(0, 0, 0);
        require(!normal.reconcile(gradual, Vector3f(0.3f, 0.4f, 0), 100, 0.5f, false),
                "Normal mode corrected instead of gradually reconciling");
        close(gradual.x, 0.003f, "Normal reconciliation was not normalized in 3D");
        close(gradual.y, 0.004f, "Normal reconciliation exceeded the BDS step");
        require(strict.reconcile(authoritative, Vector3f(0.3f, 0.4f, 0), 100, 0.5f, true),
                "Strict mode did not replace gradual adoption with a correction");
        close(authoritative.x, 0.0f, "Strict correction adopted untrusted coordinates");

        normal.reset();
        gradual = Vector3f(0, 0, 0);
        require(!normal.reconcile(gradual, Vector3f(0.6f, 0, 0), 100, 0.5f, false),
                "Acceptance threshold was incorrectly squared a second time");
        require(normal.reconcile(gradual, Vector3f(0.72f, 0, 0), 101, 0.5f, false),
                "Normal mode accepted an error above the squared-distance threshold");
        normal.reset();
        for (uint64_t tick = 100; tick < 110; ++tick) {
            gradual = Vector3f(0, 0, 0);
            require(!normal.reconcile(gradual, Vector3f(0.1f, 0, 0), tick, 0.5f, false),
                    "Gradual reconciliation exhausted its budget too early");
        }
        gradual = Vector3f(0, 0, 0);
        require(normal.reconcile(gradual, Vector3f(0.1f, 0, 0), 110, 0.5f, false),
                "Persistent drift exceeded ten BDS adjustments without a correction");

        strict.reset();
        authoritative = Vector3f(0, 0, 0);
        strict.onMovementEffect(100);
        require(!strict.reconcile(authoritative, Vector3f(0.1f, 0, 0), 120, 0.5f, true),
                "Movement effect grace ended before 20 ticks");
        close(authoritative.x, 0.0f, "Movement effect grace adopted the client position");
        require(strict.reconcile(authoritative, Vector3f(0.1f, 0, 0), 121, 0.5f, true),
                "Movement effect grace did not expire");
        strict.reset();
        strict.onMovementEffect(100);
        require(strict.reconcile(authoritative, Vector3f(1, 0, 0), 101, 0.5f, true),
                "Movement effect grace bypassed the large-error check");
        normal.reset();
        gradual = Vector3f(0, 0, 0);
        require(!normal.reconcile(gradual, Vector3f(0, 0, 0), 100, 0.0f, false),
                "Zero threshold rejected an exact match in normal mode");
        require(normal.reconcile(gradual, Vector3f(0.001f, 0, 0), 101, 0.0f, false),
                "Zero threshold accepted displacement");
    }

    void checkStrictReplay(ServerNetworkHandler &owner) {
        const auto path = ServerPaths::getRoot() / "simulation.properties";
        std::ofstream(path) << "server-authoritative-movement-strict=true\n"
                              "player-position-acceptance-threshold=0.000001\n";
        owner.setProperties(PropertiesSettings(path.string()));
        NetworkIdentifier id;
        ServerPlayer immediate(id, 2002);
        ServerPlayer delayed(id, 2003);
        Level &level = owner.getLevelFor(immediate);
        level.getChunk(2, 0);
        level.getBlockActors().loadChunk(2, 0, {}, owner.getCodecContext());
        for (int x = 33; x <= 40; ++x)
            for (int y = 245; y <= 253; ++y)
                for (int z = 0; z <= 1; ++z)
                    level.setBlockState(x, y, z, BlockState("minecraft:air"));
        level.setBlockState(34, 248, 0, BlockState("minecraft:stone"));
        level.setBlockState(38, 248, 0, BlockState("minecraft:stone"));
        immediate.setLoginState(ServerPlayer::LoginState::StartGameSent);
        delayed.setLoginState(ServerPlayer::LoginState::StartGameSent);
        immediate.setPosition(Vector3f(34.5f, 250.0f, 0.5f));
        delayed.setPosition(Vector3f(38.5f, 250.0f, 0.5f));
        for (int64_t tick = 1; tick <= 10; ++tick) {
            immediate.getMovementInputs().enqueue(input(tick, 34.5f));
            MovementInputProbe::step(owner, immediate, tick);
        }
        for (int64_t tick = 1; tick < 10; ++tick)
            MovementInputProbe::step(owner, delayed, tick);
        for (int64_t tick = 1; tick <= 10; ++tick)
            delayed.getMovementInputs().enqueue(input(tick, 38.5f));
        MovementInputProbe::step(owner, delayed, 10, 1000);
        require(immediate.hasSimulatedPosition() && delayed.hasSimulatedPosition(),
                "Replay equivalence fixture skipped strict physics");
        require(immediate.getSimulatedVelocity().y < 0.0f && immediate.getPosition().y < 250.0f,
                "Replay equivalence fixture did not simulate falling");
        require(std::fabs(immediate.getPosition().y - delayed.getPosition().y) < 0.00001f
                && std::fabs(immediate.getSimulatedVelocity().y - delayed.getSimulatedVelocity().y) < 0.00001f,
                "Delayed replay diverged from sequential strict simulation");
        require(immediate.isOnGround() && delayed.isOnGround(), "Replay did not preserve landing collision state");
        require(delayed.getMovementInputs().lastProcessedTick() == 10, "Catchup used the server tick as the input tick");
    }

    void checkServerPipeline() {
        ServerNetworkHandler owner("Input regression", "", 2);
        const auto path = ServerPaths::getRoot() / "timeline.properties";
        std::ofstream(path) << "simulation=false\n";
        owner.setProperties(PropertiesSettings(path.string()));
        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 1001).first->second;
        player.setLoginState(ServerPlayer::LoginState::StartGameSent);
        player.setGameType((int32_t) GameType::Creative);
        player.setPosition(Vector3f(0.5f, 250.0f, 0.5f));
        MovementInputProbe::receive(owner, id, input(100, 1.5f));
        MovementInputProbe::receive(owner, id, input(101, 2.5f));
        MovementInputProbe::receive(owner, id, input(102, 3.5f));
        require(player.getPosition().x == 0.5f && player.getMovementInputs().pendingCount() == 3,
                "Receiving a packet still executed movement immediately");
        MovementInputProbe::step(owner, player, 1);
        require(player.getPosition().x == 2.5f && player.getMovementInputs().lastProcessedTick() == 101,
                "First tick did not apply the credited input");
        MovementInputProbe::step(owner, player, 2);
        require(player.getPosition().x == 3.5f && player.getMovementInputs().pendingCount() == 0,
                "Following tick did not consume the deferred tail");
        MovementInputProbe::step(owner, player, 12);
        for (int64_t tick = 103; tick <= 108; ++tick)
            MovementInputProbe::receive(owner, id, input(tick, (float) (tick - 98) + 0.5f));
        MovementInputProbe::step(owner, player, 13);
        require(player.getMovementInputs().lastProcessedTick() == 104
                && player.getMovementInputs().pendingCount() == 4, "Normal frame did not bound catchup work");
        MovementInputProbe::step(owner, player, 14, 1000);
        require(player.getMovementInputs().lastProcessedTick() == 108
                && player.getMovementInputs().pendingCount() == 0, "Catchup did not consume accumulated credits");
        MovementInputProbe::receive(owner, id, input(109));
        MovementInputProbe::receive(owner, id, input(110));
        player._setVehicleId(9001);
        MovementInputProbe::step(owner, player, 15, 1000);
        require(player.getMovementInputs().lastProcessedTick() == 109
                && player.getMovementInputs().pendingCount() == 1,
                "Catchup consumed rider inputs before the vehicle could travel");
        player._setVehicleId(0);
        player.setSimulatedPosition(player.getPosition());
        player.markTeleported();
        require(player.getMovementInputs().pendingCount() == 0 && !player.hasSimulatedPosition(),
                "Teleport preserved stale queued coordinates or simulation state");
        MovementInputProbe::receive(owner, id, input(111));
        player.setAwaitingDimensionAck(true);
        MovementInputProbe::receive(owner, id, input(112));
        require(player.getMovementInputs().pendingCount() == 0, "Dimension transition accepted queued old-world movement");
        player.setAwaitingDimensionAck(false);
        MovementInputProbe::receive(owner, id, input(113));
        player.setDead(true);
        MovementInputProbe::step(owner, player, 16);
        require(player.getMovementInputs().pendingCount() == 0, "Death retained pending movement");
        checkStrictReplay(owner);
    }

    void checkSimulationSettings() {
        ServerNetworkHandler owner("Simulation settings regression", "", 1);
        const auto path = ServerPaths::getRoot() / "switch.properties";
        std::ofstream(path) << "simulation=invalid\n";
        const PropertiesSettings invalid(path.string());
        require(invalid.getSimulation() && invalid.getInvalidProperties().size() == 1,
                "Invalid simulation setting did not fall back to enabled");
        for (const std::string configuration: {"", "simulation=true\n",
                                               "simulation=true\nserver-authoritative-movement-strict=true\n",
                                               "simulation=false\nserver-authoritative-movement-strict=true\n"}) {
            std::ofstream(path) << configuration;
            const PropertiesSettings settings(path.string());
            owner.setProperties(settings);
            NetworkIdentifier id;
            ServerPlayer player(id, 4001);
            Level &level = owner.getLevelFor(player);
            level.getChunk(4, 0);
            level.getBlockActors().loadChunk(4, 0, {}, owner.getCodecContext());
            for (int x = 65; x <= 68; ++x)
                for (int z = 1; z <= 4; ++z)
                    for (int y = 249; y <= 252; ++y)
                        level.setBlockState(x, y, z, BlockState(y == 249 ? "minecraft:stone" : "minecraft:air"));
            player.setPosition(Vector3f(66.5f, 250.0f, 2.5f));
            player.setOnGround(true);
            player.setSimulatedPosition(player.getPosition());
            player.setSimulatedVelocity(Vector3f(0, -0.0784f, 0));
            PlayerAuthInputPacket packet;
            packet.mTick = 100;
            packet.mDelta = Vector3f(100, 100, 100);
            Vector3f feet(66.6f, 250.0f, 2.5f);
            PlayerMovementSimulator::apply(owner, id, player, packet, feet, false);
            if (!settings.getSimulation()) {
                require(!player.hasSimulatedPosition() && !player.hasSimulatedVelocity(),
                        "simulation=false did not clear replay state when strict was enabled");
                require(feet.x == 66.6f, "Disabled simulation changed the reported coordinates");
                continue;
            }
            require(player.hasSimulatedPosition() && player.hasSimulatedVelocity(),
                    "Enabled simulation still depended on the strict setting");
            const float expected = settings.getServerAuthoritativeMovementStrict() ? 66.5f : 66.505f;
            require(std::fabs(feet.x - expected) < 0.00001f,
                    "Simulation did not apply the configured strict or gradual BDS position policy");
            require(player.getSimulatedVelocity().x == 0.0f && player.getMotion().x == 0.0f,
                    "Client delta replaced authoritative velocity");
            player.setSimulatedPosition(player.getPosition());
            player.setSimulatedVelocity(Vector3f(0, -0.0784f, 0));
            feet = Vector3f(66.502f, 250.0f, 2.5f);
            packet.mTick = 200;
            PlayerMovementSimulator::apply(owner, id, player, packet, feet, false);
            require(feet.x == 66.502f && player.getSimulatedVelocity().x == 0.0f
                    && player.getMotion().x == 0.0f,
                    "Accepting a close position also trusted the client velocity");
        }
    }

    // BDS 1.26.10: ground 0x140E104A0, water 0x140F1E690/0x14103D110,
    // levitation 0x14104F680, flight 0x14104F280/0x140F1D980, glide 0x140C66F70.
    void checkTravelPhysics() {
        ServerNetworkHandler owner("Travel regression", "", 1);
        const auto path = ServerPaths::getRoot() / "travel.properties";
        std::ofstream(path) << "server-authoritative-movement-strict=true\n"
                              "player-position-acceptance-threshold=0\n";
        owner.setProperties(PropertiesSettings(path.string()));
        NetworkIdentifier id;
        ServerPlayer locator(id, 3000);
        Level &level = owner.getLevelFor(locator);
        level.getChunk(3, 0);
        level.getBlockActors().loadChunk(3, 0, {}, owner.getCodecContext());
        int64_t nextId = 3001;
        const auto terrain = [&](const BlockState &floor, const BlockState &body) {
            for (int x = 49; x <= 52; ++x)
                for (int z = 1; z <= 4; ++z)
                    for (int y = 247; y <= 254; ++y)
                        level.setBlockState(x, y, z, y == 249 ? floor : y >= 250 && y <= 252
                                                                      ? body : BlockState("minecraft:air"));
        };
        const auto makePlayer = [&](bool grounded = true) {
            auto player = std::make_unique<ServerPlayer>(id, nextId++);
            player->getAttributes() = ActorAttributes::createPlayerDefaults();
            player->setPosition(Vector3f(50.5f, 250.0f, 2.5f));
            player->setOnGround(grounded);
            player->setSimulatedPosition(player->getPosition());
            player->setSimulatedVelocity(Vector3f(0, 0, 0));
            return player;
        };
        const auto step = [&](ServerPlayer &player, std::vector<int32_t> flags = {}, bool sprinting = false,
                              float forward = 1.0f) {
            PlayerAuthInputPacket packet;
            packet.mMotionY = forward;
            packet.mInputData = std::move(flags);
            packet.mTick = 1;
            packet.mPosition = player.getPosition();
            Vector3f feet = player.getPosition();
            PlayerMovementSimulator::apply(owner, id, player, packet, feet, sprinting);
            require(player.hasSimulatedVelocity(), "Travel unexpectedly bypassed server simulation");
            player.setPosition(feet);
        };
        const auto close = [&](float actual, float expected, const char *message) {
            if (std::fabs(actual - expected) >= 0.000002f)
                throw std::runtime_error(std::string(message) + ": got " + std::to_string(actual)
                                         + ", expected " + std::to_string(expected));
        };
        const BlockState air("minecraft:air");
        const BlockState stone("minecraft:stone");
        terrain(stone, air);
        auto walk = makePlayer();
        step(*walk);
        close(walk->getSimulatedVelocity().z, 0.053508f, "BDS ground acceleration or drag differs");
        auto sprint = makePlayer();
        step(*sprint, {}, true);
        close(sprint->getSimulatedVelocity().z, 0.0695604f, "BDS sprint acceleration differs");

        auto stopped = makePlayer();
        MobEffectInstance slowness;
        slowness.mId = MobEffectId::Slowness;
        slowness.mAmplifier = 6;
        slowness.mDuration = 200;
        stopped->addEffect(slowness);
        step(*stopped);
        close(stopped->getSimulatedVelocity().z, 0.0f, "Zero movement speed regained default acceleration");
        auto fast = makePlayer();
        MobEffectInstance speed;
        speed.mId = MobEffectId::Speed;
        speed.mDuration = 200;
        fast->addEffect(speed);
        step(*fast);
        close(fast->getSimulatedVelocity().z, 0.0642096f, "Speed effect did not reach the simulator");

        terrain(BlockState("minecraft:ice"), air);
        Tag snowStates = Tag::ofCompound();
        snowStates.putInt("height", 0);
        level.setBlockState(50, 250, 2, BlockState("minecraft:snow_layer", snowStates));
        auto snow = makePlayer();
        snow->setPosition(Vector3f(50.5f, 250.125f, 2.5f));
        snow->setSimulatedPosition(snow->getPosition());
        step(*snow);
        close(snow->getSimulatedVelocity().z, 0.053508f, "Ground probe selected ice below the snow layer");

        for (const char *surface: {"minecraft:soul_sand", "minecraft:soul_soil"}) {
            terrain(BlockState(surface), air);
            for (int enchantmentLevel = 1; enchantmentLevel <= 3; ++enchantmentLevel) {
                auto soul = makePlayer();
                ItemStack boots = owner.createItemStack("minecraft:diamond_boots", 1);
                ItemEnchantments::write(boots, {{EnchantmentIds::SOUL_SPEED, enchantmentLevel}});
                soul->getInventory().setArmor(PlayerInventory::ARMOR_FEET, boots);
                step(*soul);
                const float expected = 0.13f + 0.0105f * (float) enchantmentLevel;
                close(soul->getAttributes().get("minecraft:movement"), expected, "Soul Speed modifier differs");
                close(soul->getSimulatedVelocity().z, expected * 0.98f * 0.54600006f,
                      "Soul Speed was omitted or applied twice");
                soul->setPosition(Vector3f(50.5f, 253.0f, 2.5f));
                soul->setSimulatedPosition(soul->getPosition());
                soul->setOnGround(false);
                step(*soul);
                close(soul->getAttributes().get("minecraft:movement"), expected, "Jumping removed Soul Speed over air");
                terrain(stone, air);
                soul->setPosition(Vector3f(50.5f, 250.0f, 2.5f));
                soul->setSimulatedPosition(soul->getPosition());
                step(*soul);
                close(soul->getAttributes().get("minecraft:movement"), 0.1f, "Soul Speed survived another surface");
                terrain(BlockState(surface), air);
            }
        }

        terrain(stone, WaterBlock::source());
        auto water = makePlayer(false);
        step(*water);
        close(water->getSimulatedVelocity().z, 0.01568f, "Water acceleration or drag differs");
        close(water->getSimulatedVelocity().y, -0.005f, "Player water gravity differs");
        auto waterLevitation = makePlayer(false);
        MobEffectInstance waterEffect;
        waterEffect.mId = MobEffectId::Levitation;
        waterEffect.mAmplifier = 1;
        waterEffect.mDuration = 200;
        waterLevitation->addEffect(waterEffect);
        step(*waterLevitation);
        close(waterLevitation->getSimulatedVelocity().y, 0.015f, "Water drag, levitation and gravity order differs");
        auto swimming = makePlayer(false);
        step(*swimming, {(int32_t) PlayerAuthInputData::StartSwimming});
        close(swimming->getSimulatedVelocity().y, 0.0f, "Swimming incorrectly applied sinking gravity");
        swimming->getFlags().set(ActorFlag::Swimming, true);
        step(*swimming);
        close(swimming->getSimulatedVelocity().y, 0.0f, "Persistent swimming state was lost");
        step(*swimming, {(int32_t) PlayerAuthInputData::StopSwimming});
        close(swimming->getSimulatedVelocity().y, -0.005f, "StopSwimming failed to restore sinking gravity");
        auto depth = makePlayer();
        ItemStack boots = owner.createItemStack("minecraft:diamond_boots", 1);
        ItemEnchantments::write(boots, {{EnchantmentIds::DEPTH_STRIDER, 3}});
        depth->getInventory().setArmor(PlayerInventory::ARMOR_FEET, boots);
        step(*depth);
        close(depth->getSimulatedVelocity().z, 0.053508f, "Depth Strider ground interpolation differs");
        auto floatingDepth = makePlayer(false);
        floatingDepth->getInventory().setArmor(PlayerInventory::ARMOR_FEET, boots);
        step(*floatingDepth);
        close(floatingDepth->getSimulatedVelocity().z, 0.0395724f, "Airborne Depth Strider was not halved");

        terrain(stone, LavaBlock(BlockState("minecraft:lava")).makeState(0));
        auto lava = makePlayer(false);
        step(*lava);
        close(lava->getSimulatedVelocity().z, 0.0098f, "Lava acceleration or drag differs");
        close(lava->getSimulatedVelocity().y, -0.02f, "Lava gravity differs");

        terrain(air, air);
        auto levitating = makePlayer(false);
        MobEffectInstance levitation;
        levitation.mId = MobEffectId::Levitation;
        levitation.mAmplifier = 1;
        levitation.mDuration = 200;
        levitating->addEffect(levitation);
        step(*levitating);
        close(levitating->getSimulatedVelocity().y, 0.02f, "BDS levitation integration differs");
        levitating->setMotion(levitating->getSimulatedVelocity());
        levitating->tickEffects();
        close(levitating->getMotion().y, 0.02f, "Levitation was integrated twice in one server tick");

        auto flying = makePlayer(false);
        flying->setGameType((int32_t) GameType::Creative);
        flying->setFlying(true);
        step(*flying, {(int32_t) PlayerAuthInputData::Ascend}, true);
        close(flying->getSimulatedVelocity().z, 0.08918f, "Flying sprint did not double the advertised fly speed");
        close(flying->getSimulatedVelocity().y, 0.09f, "Vertical flight input or drag differs");
        flying->setSimulatedVelocity(Vector3f(0, 0.2f, 0));
        step(*flying, {}, false, 0.0f);
        close(flying->getSimulatedVelocity().y, 0.045f, "Mouse idle flight damping differs");
        terrain(stone, air);
        auto creative = makePlayer();
        creative->setGameType((int32_t) GameType::Creative);
        step(*creative);
        close(creative->getSimulatedVelocity().z, 0.053508f, "Grounded creative bypassed walking physics");

        terrain(air, air);
        auto gliding = makePlayer(false);
        gliding->getInventory().setArmor(PlayerInventory::ARMOR_TORSO,
                                         owner.createItemStack(ElytraItem::IDENTIFIER, 1));
        step(*gliding, {(int32_t) PlayerAuthInputData::StartGliding});
        close(gliding->getSimulatedVelocity().y, -0.01764f, "BDS glide lift differs");
        close(gliding->getSimulatedVelocity().z, 0.001782f, "Ground movement input leaked into gliding");
        gliding->getFlags().set(ActorFlag::Gliding, true);
        step(*gliding);
        require(gliding->getSimulatedVelocity().z < 0.01f, "Persistent gliding fell back to air walking");
        step(*gliding, {(int32_t) PlayerAuthInputData::StopGliding});
        require(gliding->getSimulatedVelocity().y < -0.1f, "StopGliding did not restore gravity");

        auto initial = makePlayer(false);
        initial->clearSimulatedVelocity();
        PlayerAuthInputPacket untrusted;
        untrusted.mDelta = Vector3f(100, 100, 100);
        Vector3f feet = initial->getPosition();
        PlayerMovementSimulator::apply(owner, id, *initial, untrusted, feet, false);
        close(initial->getSimulatedVelocity().z, 0.0f, "Client delta seeded authoritative velocity after reset");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
            / ("falcon-input-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        checkTimeline();
        checkPositionReconciliation();
        checkServerPipeline();
        checkSimulationSettings();
        checkTravelPhysics();
    } catch (const std::exception &error) {
        std::clog << "Movement input regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
