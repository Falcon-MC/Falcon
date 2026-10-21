#include "World/Actor/Movement/ServerPlayerMovementComponent.h"
#include "World/Actor/Movement/PlayerMovementSimulator.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Movement/Systems/MovementInputSystem.h"
#include "World/Actor/Movement/Systems/SwimControlSystem.h"
#include "World/Actor/Movement/Systems/PlayerPredictionSystem.h"
#include "World/Actor/Movement/Systems/ClientAcceptanceSystem.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"
#include "Network/Handler/MovementHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/WaterBlock.h"
#include "World/Level/Block/LavaBlock.h"
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Item/ElytraItem.h"
#include "Protocol/Packets/PlayerAuthInputPacket.h"
#include "Protocol/Types/StartGameTypes.h"
#include "Core/Debug/BedrockLog.h"

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
        // These fixtures exercise movement; chunk streaming has its own lifecycle tests.
        player.setLastChunkPosition((int32_t) std::floor(player.getPosition().x) >> 4,
                                    (int32_t) std::floor(player.getPosition().z) >> 4);
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

    void checkSprintAttributeSynchronization() {
        auto attributes = ActorAttributes::createPlayerDefaults();
        const auto movement = [](const std::vector<AttributeData> &values) {
            for (const auto &value: values)
                if (value.mName == "minecraft:movement")
                    return value;
            throw std::runtime_error("Missing movement attribute in player update");
        };
        for (float speed: {0.1f, 0.14f, 0.07f}) {
            attributes.set("minecraft:movement", speed);
            const auto sprint = movement(attributes.getPlayerUpdate(true));
            require(std::fabs(sprint.mValue - speed * 1.3f) < 0.000001f
                    && sprint.mModifiers.size() == 1
                    && sprint.mModifiers.front().mId == "d208fc00-42aa-4aad-9276-d5446530de43"
                    && sprint.mModifiers.front().mOperation == AttributeOperation::MultiplyTotal
                    && sprint.mModifiers.front().mOperand == 2
                    && std::fabs(sprint.mModifiers.front().mAmount - 0.3f) < 0.000001f,
                    "Sprint update omitted the BDS movement modifier or effective speed");
            const auto walk = movement(attributes.getPlayerUpdate(false));
            require(walk.mValue == speed && walk.mModifiers.empty()
                    && attributes.get("minecraft:movement") == speed,
                    "Stopping sprint retained the client boost or changed the server base speed");
        }
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

    void checkPredictionVelocityFeedback() {
        ServerNetworkHandler owner("Prediction feedback regression", "", 1);
        NetworkIdentifier id;
        ServerPlayer player(id, 7400);
        PlayerAuthInputPacket packet;
        packet.mTick = 100;
        Vector3f reported(10.3f, 80.4f, 20.0f);
        PlayerMovementContext context{owner, id, player, owner.getLevelFor(player), {}, {packet}, {}, {},
                                      {reported, player.getMovementValidator()}, {}};
        context.state.simulated = Vector3f(10.0f, 80.0f, 20.0f);
        context.state.velocity = Vector3f(0.1f, 0.2f, 0.3f);
        PlayerPredictionSystem::tick(context);
        require(reported.x == 10.0f && reported.y == 80.0f && reported.z == 20.0f,
                "Soft reconciliation moved the current physical position instead of steering velocity");
        require(std::fabs(player.getSimulatedVelocity().x - 0.103f) < 0.000005f
                && std::fabs(player.getSimulatedVelocity().y - 0.204f) < 0.000005f
                && player.getSimulatedVelocity().z == 0.3f,
                "BDS acceptPos displacement was not added to the next tick velocity");
        player.getMovementValidator().reset();
        reported = Vector3f(10.001f, 79.998f, 20.003f);
        context.state.velocity = Vector3f(0.1f, 0.2f, 0.3f);
        PlayerPredictionSystem::tick(context);
        require(reported.x == 10.0f && reported.y == 80.0f && reported.z == 20.0f
                && std::fabs(player.getSimulatedVelocity().x - 0.101f) < 0.000005f
                && std::fabs(player.getSimulatedVelocity().y - 0.198f) < 0.000005f
                && std::fabs(player.getSimulatedVelocity().z - 0.303f) < 0.000005f,
                "Fine-threshold adoption bypassed velocity feedback or lost a signed axis");
        player.getMovementValidator().reset();
        player.getMovementValidator().onMovementEffect(100);
        reported = Vector3f(10.3f, 80.4f, 20.0f);
        context.state.velocity = Vector3f(0.1f, 0.2f, 0.3f);
        PlayerPredictionSystem::tick(context);
        require(player.getSimulatedVelocity().x == 0.1f && player.getSimulatedVelocity().y == 0.2f,
                "Movement-effect grace period still applied acceptance feedback");
        player.getMovementValidator().reset();
        context.prediction.strict = true;
        reported = Vector3f(10.3f, 80.4f, 20.0f);
        PlayerPredictionSystem::tick(context);
        require(player.getSimulatedVelocity().x == 0.1f && player.getSimulatedVelocity().y == 0.2f,
                "A rejected strict-mode position contaminated authoritative velocity");

        player.getMovementValidator().reset();
        context.prediction.strict = false;
        context.state.simulated = Vector3f(10.0f, 80.0f, 20.0f);
        context.state.velocity = Vector3f(0.1f, 0.0f, 0.0f);
        Vector3f clientPosition = context.state.simulated;
        float clientVelocity = 0.11f;
        // A small initial speed mismatch must converge instead of exhausting the adjustment budget.
        for (int tick = 100; tick < 140; ++tick) {
            packet.mTick = tick;
            clientVelocity += 0.049f;
            clientPosition.x += clientVelocity;
            clientVelocity *= 0.91f;
            context.state.velocity.x += 0.049f;
            context.state.simulated.x += context.state.velocity.x;
            context.state.velocity.x *= 0.91f;
            reported = clientPosition;
            auto comparison = player.getMovementValidator();
            Vector3f checked = context.state.simulated;
            require(!comparison.reconcile(checked, reported, (uint64_t) tick, 0.5f, false),
                    "Small flight speed drift exhausted BDS's acceptance budget instead of converging");
            PlayerPredictionSystem::tick(context);
        }
        require(std::fabs(context.state.simulated.x - clientPosition.x) < 0.005f,
                "Flight reconciliation failed to converge after the initial speed mismatch");
    }

    void checkCorrectionDiagnostics() {
        PlayerMovementValidator validator;
        MovementCorrectionDetails details;
        Vector3f position;
        require(validator.reconcile(position, Vector3f(1, 2, 3), 20, 0.5f, false, &details)
                && std::string(details.reason) == "position_threshold"
                && details.error.x == 1.0f && details.error.y == 2.0f && details.error.z == 3.0f
                && details.distanceSquared == 14.0f && details.threshold == 0.5f
                && details.previousCorrectionTick == 0,
                "Large-error diagnostics lost the actual comparison or axis error");
        require(!validator.reconcile(position, Vector3f(1, 2, 3), 21, 0.5f, false, &details)
                && details.reason == nullptr, "Correction cooldown retained a stale reason");
        validator.reset();
        require(validator.reconcile(position, Vector3f(0.1f, 0, 0), 20, 0.5f, true, &details)
                && std::string(details.reason) == "strict_mismatch"
                && std::fabs(details.fineThreshold - 0.000025f) < 0.00000001f,
                "Strict-mode diagnostics reported the broad threshold as the trigger");
        validator.reset();
        for (uint64_t tick = 20; tick < 30; ++tick) {
            position = Vector3f();
            require(!validator.reconcile(position, Vector3f(0.1f, 0, 0), tick, 0.5f, false, &details)
                    && details.reason == nullptr, "Gradual adjustment was mislabeled as a correction");
        }
        position = Vector3f();
        require(validator.reconcile(position, Vector3f(0.1f, 0, 0), 30, 0.5f, false, &details)
                && std::string(details.reason) == "adjustment_limit" && details.adjustments == 10,
                "Accumulated-drift diagnostics did not identify the exhausted adjustment budget");

        class CorrectionLog final : public ILogEndPoint {
        public:
            void log(const LogDetails &entry) override {
                if (entry.mMessage.find("[MovementCorrection]") != std::string::npos)
                    messages.push_back(entry.mMessage);
            }

            std::vector<std::string> messages;
        };
        const auto log = std::make_shared<CorrectionLog>();
        BedrockLog::flush();
        BedrockLog::addEndPoint(log);
        ServerNetworkHandler owner("Correction logging regression", "", 1);
        NetworkIdentifier id;
        ServerPlayer player(id, 7200);
        player.setName("CorrectionProbe");
        player.setGameType((int32_t) GameType::Creative);
        player.setFlying(true);
        player.setPosition(Vector3f(134.5f, 250.0f, 2.5f));
        Level &level = owner.getLevelFor(player);
        level.getChunk(8, 0);
        level.getBlockActors().loadChunk(8, 0, {}, owner.getCodecContext());
        PlayerAuthInputPacket packet;
        packet.mTick = 20;
        packet.mInputData = {(int32_t) PlayerAuthInputData::JumpCurrentRaw,
                             (int32_t) PlayerAuthInputData::InternalUpdate};
        Vector3f reported(135.5f, 252.0f, 5.5f);
        PlayerMovementSimulator::apply(owner, id, player, packet, reported, false);
        BedrockLog::flush();
        BedrockLog::removeEndPoint(log);
        require(log->messages.size() == 1, "A correction did not produce exactly one diagnostic record");
        for (const char *field: {"player=CorrectionProbe", "reason=position_threshold", "tick=20 ",
                                "error=(1,2,3)", "distance_squared=14", "reported=(135.5,252,5.5)",
                                "predicted=(134.5,250,2.5)", "flags=0000000000000002:2000000000000000",
                                "fly=1", "collision=(0,0,0)", "pending_inputs=0"})
            require(log->messages.front().find(field) != std::string::npos,
                    "Movement correction diagnostic omitted or overwrote a measured field");
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
        immediate.setLoginState(ServerPlayer::LoginState::Spawned);
        delayed.setLoginState(ServerPlayer::LoginState::Spawned);
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

    // BDS jump 0x140EAFAF0, cooldown 0x140EB0DA0, ground/air travel and gravity.
    void checkGroundJumpTrajectory() {
        for (bool strict: {false, true}) {
            ServerNetworkHandler owner("Ground trajectory regression", "", 1);
            const auto path = ServerPaths::getRoot() / "ground-trajectory.properties";
            std::ofstream(path) << "simulation=true\nserver-authoritative-movement-strict="
                                << (strict ? "true" : "false") << "\n";
            owner.setProperties(PropertiesSettings(path.string()));
            NetworkIdentifier id;
            ServerPlayer player(id, 5000);
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            Level &level = owner.getLevelFor(player);
            for (int chunkZ = 0; chunkZ < 4; ++chunkZ) {
                level.getChunk(4, chunkZ);
                level.getBlockActors().loadChunk(4, chunkZ, {}, owner.getCodecContext());
            }
            for (int x = 69; x <= 72; ++x)
                for (int z = 0; z < 64; ++z)
                    for (int y = 249; y <= 254; ++y)
                        level.setBlockState(x, y, z, BlockState(y == 249 ? "minecraft:stone" : "minecraft:air"));
            Vector3f expected(70.5f, 250.0f, 2.5f);
            Vector3f velocity;
            bool grounded = true;
            int jumpDelay = 0;
            int jumps = 0;
            player.setPosition(expected);
            player.setOnGround(true);
            for (int tick = 1; tick <= 60; ++tick) {
                const bool wasGrounded = grounded;
                if (jumpDelay > 0)
                    --jumpDelay;
                const bool jumpHeld = tick > 5;
                if (jumpHeld && grounded && jumpDelay == 0) {
                    velocity.y = std::max(0.42f, velocity.y);
                    velocity.z += 0.2f;
                    jumpDelay = 10;
                    ++jumps;
                }
                velocity.z += (grounded ? 0.13f : 0.026f) * 0.98f;
                const float requestedY = expected.y + velocity.y;
                grounded = requestedY < 250.0f || (grounded && velocity.y == 0.0f);
                expected.y = std::max(250.0f, requestedY);
                expected.z += velocity.z;
                if (grounded)
                    velocity.y = 0.0f;
                velocity.y = (velocity.y - 0.08f) * 0.98f;
                velocity.z *= wasGrounded ? 0.54600006f : 0.91f;
                PlayerAuthInputPacket packet;
                packet.mTick = 100 + tick;
                packet.mPosition = Vector3f(expected.x, expected.y + 1.62f, expected.z);
                packet.mMotionY = 1.0f;
                packet.mInputData = {(int32_t) PlayerAuthInputData::Sprinting};
                if (jumpHeld)
                    packet.mInputData.push_back((int32_t) PlayerAuthInputData::Jumping);
                if (tick == 6)
                    packet.mInputData.push_back((int32_t) PlayerAuthInputData::StartJumping);
                player.getMovementInputs().enqueue(packet);
                MovementInputProbe::step(owner, player, tick);
                require(player.isOnGround() == grounded,
                        "Ground handler overwrote the jump collision state with a block below the player");
                require(std::fabs(player.getPosition().y - expected.y) < 0.00005f
                        && std::fabs(player.getPosition().z - expected.z) < 0.00005f
                        && std::fabs(player.getSimulatedVelocity().z - velocity.z) < 0.00001f,
                        "Held sprint-jump trajectory diverged from the BDS reference");
            }
            require(jumps >= 4, "Trajectory fixture failed to exercise held repeat jumps");
        }
    }

    void checkCreativeFlightTrajectory() {
        ServerNetworkHandler owner("Creative flight regression", "", 1);
        const auto path = ServerPaths::getRoot() / "flight.properties";
        std::ofstream(path) << "simulation=true\nserver-authoritative-movement-strict=true\n";
        owner.setProperties(PropertiesSettings(path.string()));
        NetworkIdentifier id;
        ServerPlayer locator(id, 7000);
        Level &level = owner.getLevelFor(locator);
        for (int chunkZ = 0; chunkZ < 4; ++chunkZ) {
            level.getChunk(6, chunkZ);
            level.getBlockActors().loadChunk(6, chunkZ, {}, owner.getCodecContext());
        }
        for (PlayerInputMode mode: {PlayerInputMode::Mouse, PlayerInputMode::Touch, PlayerInputMode::Gamepad}) {
            ServerPlayer player(id, 7001);
            player.setGameType((int32_t) GameType::Creative);
            player.setFlying(true);
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            Vector3f expected(102.5f, 250.0f, 2.5f);
            Vector3f velocity;
            player.setPosition(expected);
            for (int tick = 1; tick <= 100; ++tick) {
                const bool moving = tick <= 30 || tick > 65;
                const bool sprinting = tick >= 16 && tick <= 30;
                const bool ascend = tick <= 10 || (tick >= 41 && tick <= 50);
                const bool descend = tick >= 41 && tick <= 60;
                const bool jumping = tick >= 11 && tick <= 20;
                const bool sneaking = tick >= 21 && tick <= 25;
                if (!moving && mode == PlayerInputMode::Mouse && !ascend && !descend && !jumping && !sneaking)
                    velocity.y *= 0.375f;
                if (ascend && descend)
                    velocity.y = 0.0f;
                else
                    velocity.y += (ascend ? 0.15f : 0.0f) + (jumping ? 0.05f : 0.0f)
                                  - (descend ? 0.22f : 0.0f) - (sneaking ? 0.15f : 0.0f);
                if (moving)
                    velocity.z += (sprinting ? 0.1f : 0.05f) * 0.98f;
                expected.y += velocity.y;
                expected.z += velocity.z;
                velocity.y *= 0.6f;
                velocity.z *= 0.91f * (moving ? 1.0f : mode == PlayerInputMode::Mouse ? 0.375f : 0.75f);
                PlayerAuthInputPacket packet;
                packet.mTick = tick;
                packet.mInputMode = mode;
                packet.mMotionY = moving ? 1.0f : 0.0f;
                packet.mPosition = Vector3f(expected.x, expected.y + 1.62f, expected.z);
                if (ascend)
                    packet.mInputData.push_back((int32_t) PlayerAuthInputData::WantUp);
                if (descend)
                    packet.mInputData.push_back((int32_t) PlayerAuthInputData::WantDown);
                if (jumping)
                    packet.mInputData.push_back((int32_t) PlayerAuthInputData::WantUpSlow);
                if (sneaking)
                    packet.mInputData.push_back((int32_t) PlayerAuthInputData::WantDownSlow);
                Vector3f feet = expected;
                // Isolate travel integration from prediction feedback, which has its own regression above.
                player.getMovementValidator().onMovementEffect((uint64_t) tick);
                PlayerMovementSimulator::apply(owner, id, player, packet, feet, sprinting);
                player.setPosition(feet);
                require(std::fabs(feet.y - expected.y) < 0.00005f && std::fabs(feet.z - expected.z) < 0.00005f
                        && std::fabs(player.getSimulatedVelocity().y - velocity.y) < 0.000002f
                        && std::fabs(player.getSimulatedVelocity().z - velocity.z) < 0.000002f,
                        "Creative flying trajectory differs from BDS vertical control, drag or idle friction");
            }
        }
        for (int x = 101; x <= 104; ++x)
            for (int z = 1; z <= 4; ++z)
                level.setBlockState(x, 249, z, BlockState("minecraft:stone"));
        ServerPlayer grounded(id, 7002);
        grounded.setGameType((int32_t) GameType::Creative);
        grounded.setFlying(true);
        grounded.setPosition(Vector3f(102.5f, 250.0f, 2.5f));
        grounded.setOnGround(true);
        PlayerAuthInputPacket packet;
        packet.mMotionY = 1.0f;
        Vector3f feet = grounded.getPosition();
        PlayerMovementSimulator::apply(owner, id, grounded, packet, feet, false);
        require(!grounded.isSimulatedOnGround() && feet.y == 250.0f
                && std::fabs(grounded.getSimulatedVelocity().z - 0.026754f) < 0.000002f,
                "Flying player retained ground contact or ignored first-input surface friction");
        grounded.setPosition(feet);
        PlayerMovementSimulator::apply(owner, id, grounded, packet, feet, false);
        require(std::fabs(grounded.getSimulatedVelocity().z - (0.026754f + 0.049f) * 0.91f) < 0.000002f,
                "Flying player reused ground friction after releasing ground contact");

        for (int x = 101; x <= 104; ++x)
            for (int z = 1; z <= 4; ++z)
                level.setBlockState(x, 253, z, BlockState("minecraft:stone"));
        grounded.setPosition(Vector3f(102.5f, 251.0f, 2.5f));
        grounded.setSimulatedPosition(grounded.getPosition());
        grounded.setSimulatedVelocity(Vector3f(0.0f, 0.2f, 0.0f));
        packet.mMotionY = 0.0f;
        packet.mInputData = {(int32_t) PlayerAuthInputData::WantUp};
        feet = Vector3f(102.5f, 251.2f, 2.5f);
        PlayerMovementSimulator::apply(owner, id, grounded, packet, feet, false);
        require(std::fabs(feet.y - 251.2f) < 0.00003f && grounded.getSimulatedVelocity().y == 0.0f,
                "Creative flight penetrated a ceiling or retained blocked vertical velocity");
        grounded.setPosition(Vector3f(102.5f, 250.5f, 2.5f));
        grounded.setSimulatedPosition(grounded.getPosition());
        grounded.setSimulatedVelocity(Vector3f(0.0f, 0.1f, 0.0f));
        grounded.setFlying(false);
        packet.mInputData.clear();
        feet = Vector3f(102.5f, 250.6f, 2.5f);
        PlayerMovementSimulator::apply(owner, id, grounded, packet, feet, false);
        require(std::fabs(grounded.getSimulatedVelocity().y - 0.0196f) < 0.000002f,
                "Stopping creative flight did not restore airborne gravity");
    }

    void checkAnalogInputSlowdown() {
        ServerNetworkHandler owner("Analog input regression", "", 1);
        NetworkIdentifier id;
        ServerPlayer player(id, 7299);
        PlayerAuthInputPacket packet;
        Vector3f feet;
        PlayerMovementContext context{owner, id, player, owner.getLevelFor(player), {}, {packet}, {}, {},
                                      {feet, player.getMovementValidator()}, {}};
        packet.mMotionX = 1.0f;
        packet.mMotionY = 1.0f;
        MovementInputSystem::tick(context);
        require(std::fabs(std::hypot(context.input.sideways, context.input.forward) - 0.98f) < 0.000002f,
                "Diagonal input was not normalized before damping");
        packet.mMotionX = 0.2f;
        packet.mMotionY = 0.4f;
        context.travel.sneaking = true;
        MovementInputSystem::tick(context);
        require(std::fabs(context.input.sideways - 0.0588f) < 0.000002f
                && std::fabs(context.input.forward - 0.1176f) < 0.000002f,
                "Crouch slowdown clipped analog axes instead of scaling their magnitude");
        context.abilities.swimming = true;
        MovementInputSystem::tick(context);
        require(std::fabs(context.input.sideways - 0.196f) < 0.000002f
                && std::fabs(context.input.forward - 0.392f) < 0.000002f,
                "Swimming inherited the crouched input slowdown");
        packet.mRotation.x = -30.0f;
        context.state.velocity.y = 0.1f;
        SwimControlSystem::tick(context);
        require(context.state.velocity.y == 0.1f,
                "Swim control ran without BDS's required water contact");
        context.contact.waterTravel = true;
        SwimControlSystem::tick(context);
        require(context.state.velocity.y == 0.0f,
                "Swim control ignored the dry head block while the body was still in water");
        context.input.jumpHeld = true;
        context.state.velocity.y = 0.1f;
        SwimControlSystem::tick(context);
        require(context.state.velocity.y == 0.1f,
                "Swim control ran despite the jumping exclusion");
    }

    void checkConfinedSwimmingPose() {
        ServerNetworkHandler owner("Confined pose regression", "", 1);
        NetworkIdentifier id;
        ServerPlayer locator(id, 7360);
        Level &level = owner.getLevelFor(locator);
        level.getChunk(10, 0);
        level.getBlockActors().loadChunk(10, 0, {}, owner.getCodecContext());
        for (bool wet: {false, true}) {
            for (int x = 164; x <= 168; ++x)
                for (int z = 0; z <= 4; ++z) {
                    level.setBlockState(x, 249, z, BlockState("minecraft:stone"));
                    level.setBlockState(x, 250, z, wet ? WaterBlock::source() : BlockState("minecraft:air"));
                    level.setBlockState(x, 251, z, BlockState("minecraft:ice"));
                }
            ServerPlayer player(id, 7361);
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.setPosition(Vector3f(166.5f, 250.1f, 2.5f));
            player.getFlags().set(ActorFlag::Swimming, true);
            player.getMovementValidator().onMovementEffect(100);
            PlayerAuthInputPacket packet;
            packet.mPosition = Vector3f(166.5f, 251.72f, 2.5f);
            packet.mInputData = {(int32_t) PlayerAuthInputData::StopSwimming};
            for (int tick = 100; tick <= 102; ++tick) {
                packet.mTick = tick;
                player.getMovementInputs().enqueue(packet);
                MovementInputProbe::step(owner, player, tick);
                require(player.getFlags().get(ActorFlag::Swimming) == wet
                        && player.getFlags().get(ActorFlag::Crawling) == !wet
                        && !player.isSimulatedSneaking(),
                        "StopSwimming expanded a confined swimmer instead of preserving a valid horizontal pose");
                require(player.getPosition().y < 250.4f,
                        "Changing pose moved the player through the ice ceiling");
            }
            if (!wet) {
                for (int x = 164; x <= 168; ++x)
                    for (int z = 0; z <= 4; ++z)
                        level.setBlockState(x, 251, z, BlockState("minecraft:air"));
                packet.mTick = 103;
                packet.mInputData.clear();
                player.getMovementInputs().enqueue(packet);
                MovementInputProbe::step(owner, player, 103);
                require(!player.getFlags().get(ActorFlag::Crawling),
                        "Crawling did not stop automatically when standing became possible");
            }
        }
    }

    void checkClientAcceptance() {
        ServerNetworkHandler owner("Client acceptance regression", "", 1);
        NetworkIdentifier id;
        ServerPlayer player(id, 7370);
        Level &level = owner.getLevelFor(player);
        level.getChunk(11, 0);
        level.getBlockActors().loadChunk(11, 0, {}, owner.getCodecContext());
        for (int x = 179; x <= 183; ++x)
            for (int y = 249; y <= 253; ++y)
                for (int z = 1; z <= 4; ++z)
                    level.setBlockState(x, y, z, BlockState("minecraft:air"));
        PlayerAuthInputPacket packet;
        Vector3f reported;
        const auto contextFor = [&](const Vector3f &position) {
            PlayerMovementContext context{owner, id, player, level, {}, {packet}, {}, {},
                                          {reported, player.getMovementValidator()}, {}};
            context.state.start = position;
            context.state.simulated = position;
            context.state.velocity = Vector3f(0.012f, -0.08f, 0.03f);
            context.travel.pose.mHeight = 1.8f;
            context.travel.box = MovementPhysics::boxAt(position, 1.8f);
            return context;
        };
        auto &budget = player.getMovementValidator().clientAcceptance();
        auto context = contextFor(Vector3f(180.5f, 250.0f, 2.5f));
        reported = Vector3f(180.575f, 250.0f, 2.5f);
        budget.mBudget = 0.5f;
        ClientAcceptanceSystem::tick(context);
        require(std::fabs(context.state.simulated.x - reported.x) < 0.00002f
                && context.state.velocity.x == 0.012f && context.state.velocity.y == -0.08f
                && std::fabs(budget.mBudget - 0.46f) < 0.00002f,
                "BDS air acceptance did not spend/refill its budget independently of physical velocity");
        auto context2 = contextFor(Vector3f(180.5f, 250.0f, 2.5f));
        reported = Vector3f(182.5f, 249.5f, 2.5f);
        budget.mBudget = 0.5f;
        ClientAcceptanceSystem::tick(context2);
        require(context2.state.simulated.x == 180.5f && context2.state.simulated.y == 250.0f,
                "Acceptance adopted an oversized horizontal displacement or downward client error");
        auto context3 = contextFor(Vector3f(180.5f, 250.0f, 2.5f));
        reported = Vector3f(180.58f, 250.2f, 2.58f);
        budget.mBudget = 0.3f;
        ClientAcceptanceSystem::tick(context3);
        require(std::fabs(context3.state.simulated.y - reported.y) < 0.00002f
                && std::fabs(context3.state.simulated.x - reported.x) < 0.00002f
                && context3.state.simulated.z == 2.5f && budget.mBudget < 0.056f,
                "Acceptance did not share its Manhattan budget in Y/X/Z order");
        for (int y = 250; y <= 252; ++y)
            level.setBlockState(181, y, 2, BlockState("minecraft:stone"));
        auto context4 = contextFor(Vector3f(180.7f, 250.0f, 2.5f));
        context4.travel.moved.mRequested.x = 0.5f;
        reported = Vector3f(181.3f, 250.0f, 2.5f);
        budget.mBudget = 1.0f;
        ClientAcceptanceSystem::tick(context4);
        require(context4.state.simulated.x <= 180.70002f && budget.mBudget < 0.501f,
                "Solid-shape acceptance crossed a wall or refunded a blocked correction");
        auto context5 = contextFor(Vector3f(180.5f, 250.0f, 2.5f));
        reported = Vector3f(180.575f, 250.0f, 2.5f);
        budget.mBudget = 0.5f;
        player.getMovementValidator().onMovementEffect(1);
        ClientAcceptanceSystem::tick(context5);
        require(context5.state.simulated.x == 180.5f && budget.mCooldown == 19 && budget.mBudget == 0.5f,
                "Server-initiated motion did not suspend client acceptance for the BDS cooldown");
    }

    void checkMovementTrace() {
        PlayerMovementValidator validator;
        for (uint64_t tick = 1; tick <= 100; ++tick) {
            MovementTraceFrame frame;
            frame.tick = tick;
            validator.trace().record(frame);
        }
        require(validator.trace().size() == 32 && validator.trace().at(0).tick == 69
                && validator.trace().at(31).tick == 100,
                "Movement diagnostics did not retain exactly the last 32 input ticks in order");
        validator.reset();
        require(validator.trace().size() == 0, "Movement reset retained a stale replay history");
    }

    void checkLoadingScreenMovement() {
        ServerNetworkHandler owner("Loading movement regression", "", 1);
        NetworkIdentifier id;
        ServerPlayer player(id, 7360);
        player.setLoginState(ServerPlayer::LoginState::StartGameSent);
        player.setPosition(Vector3f(-63.5f, 63.0f, 64.5f));
        PlayerAuthInputPacket packet;
        packet.mTick = 10;
        packet.mPosition = Vector3f(-63.5f, 64.62f, 64.5f);
        packet.mMotionY = 1.0f;
        packet.mDelta.y = -0.1f;
        MovementHandler::handlePlayerAuthInput(owner, id, player, packet);
        require(!player.hasSimulatedPosition() && !player.hasSimulatedVelocity()
                && !player.hasPendingMove() && player.getPosition().y == 63.0f,
                "Loading-screen input started falling or changed the authoritative spawn position");
    }

    void checkQueuedFlightRequests() {
        ServerNetworkHandler owner("Queued flight regression", "", 1);
        const auto path = ServerPaths::getRoot() / "queued-flight.properties";
        std::ofstream(path) << "simulation=true\nserver-authoritative-movement-strict=true\n";
        owner.setProperties(PropertiesSettings(path.string()));
        NetworkIdentifier id;
        ServerPlayer player(id, 7350);
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.setGameType((int32_t) GameType::Creative);
        player.setPosition(Vector3f(150.5f, 250.0f, 2.5f));
        player.getMovementValidator().onMovementEffect(100);
        Level &level = owner.getLevelFor(player);
        level.getChunk(9, 0);
        level.getBlockActors().loadChunk(9, 0, {}, owner.getCodecContext());
        PlayerAuthInputPacket packet;
        packet.mTick = 100;
        packet.mPosition = Vector3f(150.5f, 251.62f, 2.5f);
        player.getMovementInputs().enqueue(packet);
        require(MovementHandler::requestFlight(owner, id, player, true, true) && !player.isFlying(),
                "An ability request changed the state of an older queued movement input");
        packet.mTick = 101;
        packet.mInputData = {(int32_t) PlayerAuthInputData::WantUp};
        packet.mPosition.y = 251.6916f;
        player.getMovementInputs().enqueue(packet);
        MovementInputProbe::step(owner, player, 1);
        require(player.isFlying() && player.getPosition().y == 250.0f
                && std::fabs(player.getSimulatedVelocity().y + 0.0784f) < 0.000002f,
                "Flight enabled retroactively for an older queued input");
        MovementInputProbe::step(owner, player, 2);
        require(std::fabs(player.getPosition().y - 250.0716f) < 0.00003f
                && std::fabs(player.getSimulatedVelocity().y - 0.04296f) < 0.000002f,
                "Deferred flight did not apply to the next input boundary");
        packet.mTick = 102;
        packet.mPosition.y = 251.88456f;
        player.getMovementInputs().enqueue(packet);
        require(MovementHandler::requestFlight(owner, id, player, false, true) && player.isFlying(),
                "Stop flight changed an already queued flying input");
        MovementInputProbe::step(owner, player, 3);
        require(!player.isFlying() && std::fabs(player.getSimulatedVelocity().y - 0.115776f) < 0.000002f,
                "Queued flight input received fall gravity before its flight-stop boundary");

        ServerPlayerMovementComponent timeline;
        timeline.enqueue(input(100));
        timeline.deferFlightRequest(true);
        bool flying = false;
        require(!timeline.takeFlightRequest(100, false, flying)
                && timeline.takeFlightRequest(101, false, flying) && flying,
                "Discarding an input boundary lost its deferred ability request");
        timeline.deferFlightRequest(true);
        timeline.deferFlightRequest(false);
        require(timeline.takeFlightRequest(100, true, flying) && !flying
                && !timeline.hasPendingFlightRequest(),
                "Repeated ability requests at one input boundary were not coalesced");
        timeline.deferFlightRequest(true);
        timeline.clearPending();
        require(!timeline.hasPendingFlightRequest(),
                "Resetting pending movement retained an obsolete flight request");
    }

    void checkFlightInputTransitions() {
        ServerNetworkHandler owner("Flight transition regression", "", 1);
        const auto path = ServerPaths::getRoot() / "flight-transitions.properties";
        std::ofstream(path) << "simulation=true\nserver-authoritative-movement-strict=true\n";
        owner.setProperties(PropertiesSettings(path.string()));
        NetworkIdentifier id;
        ServerPlayer player(id, 7300);
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.setGameType((int32_t) GameType::Creative);
        player.setPosition(Vector3f(150.5f, 250.0f, 2.5f));
        // This fixture measures transition timing; prediction feedback is exercised separately.
        player.getMovementValidator().onMovementEffect(100);
        Level &level = owner.getLevelFor(player);
        level.getChunk(9, 0);
        level.getBlockActors().loadChunk(9, 0, {}, owner.getCodecContext());
        PlayerAuthInputPacket packet;
        packet.mTick = 100;
        packet.mInputData = {(int32_t) PlayerAuthInputData::StartFlying,
                             (int32_t) PlayerAuthInputData::WantUp,
                             (int32_t) PlayerAuthInputData::Jumping};
        packet.mPosition = Vector3f(150.5f, 251.77f, 2.5f);
        player.getMovementInputs().enqueue(packet);
        MovementInputProbe::step(owner, player, 1);
        require(player.isFlying() && std::fabs(player.getPosition().y - 250.15f) < 0.00003f
                && std::fabs(player.getSimulatedVelocity().y - 0.09f) < 0.000002f,
                "AuthInput StartFlying was not applied before simulation, or Jumping was confused with WantUpSlow");
        packet.mTick = 101;
        packet.mInputData = {(int32_t) PlayerAuthInputData::WantUp};
        packet.mPosition.y = 252.01f;
        player.getMovementInputs().enqueue(packet);
        MovementInputProbe::step(owner, player, 2);
        require(player.isFlying() && std::fabs(player.getPosition().y - 250.39f) < 0.00003f,
                "Flying state did not persist across subsequent input ticks");
        packet.mTick = 102;
        packet.mInputData = {(int32_t) PlayerAuthInputData::StopFlying};
        player.getMovementInputs().enqueue(packet);
        MovementInputProbe::step(owner, player, 3);
        require(!player.isFlying() && std::fabs(player.getSimulatedVelocity().y - 0.06272f) < 0.000002f,
                "AuthInput StopFlying did not restore gravity on its own tick");
        packet.mTick = 103;
        packet.mInputData = {(int32_t) PlayerAuthInputData::StartFlying,
                             (int32_t) PlayerAuthInputData::WantUp};
        player.getMovementInputs().enqueue(packet);
        packet.mTick = 104;
        packet.mInputData = {(int32_t) PlayerAuthInputData::StopFlying};
        player.getMovementInputs().enqueue(packet);
        MovementInputProbe::step(owner, player, 10);
        require(!player.isFlying() && player.getMovementInputs().lastProcessedTick() == 104,
                "Catch-up inputs did not apply both flight transitions in order");
        player.setGameType((int32_t) GameType::Survival);
        require(!MovementHandler::requestFlight(owner, id, player, true) && !player.isFlying(),
                "Survival AuthInput flight bypassed the existing may-fly permission check");
    }

    void checkShallowWaterExit() {
        ServerNetworkHandler owner("Water exit regression", "", 1);
        const auto path = ServerPaths::getRoot() / "water-exit.properties";
        std::ofstream(path) << "simulation=true\nserver-authoritative-movement-strict=true\n";
        owner.setProperties(PropertiesSettings(path.string()));
        NetworkIdentifier id;
        ServerPlayer locator(id, 7100);
        Level &level = owner.getLevelFor(locator);
        level.getChunk(7, 0);
        level.getBlockActors().loadChunk(7, 0, {}, owner.getCodecContext());
        for (bool ceiling: {false, true}) {
            for (int x = 116; x <= 120; ++x)
                for (int z = 0; z <= 12; ++z)
                    for (int y = 249; y <= 254; ++y)
                        level.setBlockState(x, y, z, BlockState(y == 249 || (z >= 5 && y == 250)
                                || (ceiling && y == 253) ? "minecraft:stone" : "minecraft:air"));
            for (int x = 116; x <= 120; ++x)
                for (int z = 0; z < 5; ++z)
                    level.setBlockState(x, 250, z, WaterBlock::source());
            ServerPlayer player(id, 7101);
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.setPosition(Vector3f(118.5f, 250.5f, 4.65f));
            player.setSimulatedVelocity(Vector3f(0.0f, 0.1f, 0.15f));
            PlayerAuthInputPacket packet;
            packet.mTick = 1;
            packet.mMotionY = 1.0f;
            packet.mInputData = {(int32_t) PlayerAuthInputData::Jumping};
            Vector3f feet(118.5f, 250.64f, 4.7f);
            PlayerMovementSimulator::apply(owner, id, player, packet, feet, false);
            player.setPosition(feet);
            require(player.hasHorizontalCollision() && std::fabs(feet.y - 250.64f) < 0.00003f,
                    "Water exit fixture missed the shore collision");
            require(std::fabs(player.getSimulatedVelocity().y - (ceiling ? 0.107f : 0.3f)) < 0.000002f,
                    "Liquid exit boost ignored clearance or did not run after drag");
            if (ceiling)
                continue;
            // After the boost, the contracted sensing box is dry and air gravity resumes.
            Vector3f expected = feet;
            Vector3f velocity = player.getSimulatedVelocity();
            for (int tick = 2; tick <= 5; ++tick) {
                velocity.z += 0.02f * 0.98f;
                expected.y += velocity.y;
                const bool blocked = expected.y < 251.0f && expected.z + velocity.z > 4.7f;
                expected.z = blocked ? 4.7f : expected.z + velocity.z;
                if (blocked)
                    velocity.z = 0.0f;
                velocity.y = (velocity.y - 0.08f) * 0.98f;
                velocity.z *= 0.91f;
                packet.mTick = tick;
                feet = expected;
                PlayerMovementSimulator::apply(owner, id, player, packet, feet, false);
                player.setPosition(feet);
                require(std::fabs(feet.y - expected.y) < 0.00003f && std::fabs(feet.z - expected.z) < 0.00003f,
                        "Jumping from one-block-deep water to dry land diverged from the BDS exit trajectory");
            }
            require(feet.y > 251.0f && feet.z > 4.7f, "Player failed to clear the water's shore");
        }
    }

    void checkShallowWaterWalking() {
        ServerNetworkHandler owner("Shallow walking regression", "", 1);
        NetworkIdentifier id;
        ServerPlayer locator(id, 6000);
        Level &level = owner.getLevelFor(locator);
        for (int chunkZ = 0; chunkZ < 2; ++chunkZ) {
            level.getChunk(5, chunkZ);
            level.getBlockActors().loadChunk(5, chunkZ, {}, owner.getCodecContext());
        }
        for (bool slab: {false, true}) {
            for (int x = 83; x <= 88; ++x)
                for (int z = 0; z < 32; ++z) {
                    for (int y = 249; y <= 253; ++y)
                        level.setBlockState(x, y, z, BlockState(y == 249 ? "minecraft:stone" : "minecraft:air"));
                    if (slab) {
                        level.setBlockState(x, 250, z, BlockState("minecraft:oak_slab"));
                        level.setBlockStateAtLayer(x, 250, z, 1, WaterBlock::source());
                    } else {
                        level.setBlockState(x, 250, z, WaterBlock::source());
                    }
                }
            ServerPlayer player(id, slab ? 6002 : 6001);
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            const float feetY = slab ? 250.5f : 250.0f;
            player.setPosition(Vector3f(86.5f, feetY, 3.5f));
            player.setOnGround(true);
            require(MovementHandler::checkGroundState(level, player.getPosition())
                    && !MovementHandler::checkGroundState(level, Vector3f(86.5f, feetY + 0.42f, 3.5f)),
                    "Ground probe ignored the actual stone/slab collision surface");
            require(LiquidBlocksFetch::at(level, player.getPosition()).water
                    && !LiquidBlocksFetch::at(level, player.getPosition()).eyeSubmerged,
                    "Shallow walking fixture did not have water below the head");
            float expectedZ = 3.5f;
            float velocityZ = 0.0f;
            for (int tick = 1; tick <= 30; ++tick) {
                velocityZ += 0.02f * 0.98f;
                expectedZ += velocityZ;
                velocityZ *= 0.8f;
                PlayerAuthInputPacket packet;
                packet.mTick = 200 + tick;
                packet.mMotionY = 1.0f;
                packet.mPosition = Vector3f(86.5f, feetY + 1.62f, expectedZ);
                player.getMovementInputs().enqueue(packet);
                MovementInputProbe::step(owner, player, tick);
                require(player.isOnGround() && std::fabs(player.getPosition().y - feetY) < 0.00002f
                        && std::fabs(player.getPosition().z - expectedZ) < 0.00005f
                        && std::fabs(player.getSimulatedVelocity().z - velocityZ) < 0.00001f,
                        "Walking on stone or waterlogged slabs in shallow water diverged from BDS water travel");
            }
        }
    }

    void checkTravelReplay() {
        ServerNetworkHandler owner("Travel replay regression", "", 2);
        const auto path = ServerPaths::getRoot() / "travel-replay.properties";
        std::ofstream(path) << "simulation=true\nserver-authoritative-movement-strict=true\n"
                              "player-position-acceptance-threshold=0\n";
        owner.setProperties(PropertiesSettings(path.string()));
        NetworkIdentifier id;
        ServerPlayer locator(id, 4000);
        Level &level = owner.getLevelFor(locator);
        level.getChunk(4, 0);
        level.getBlockActors().loadChunk(4, 0, {}, owner.getCodecContext());
        for (int scenario = 0; scenario < 5; ++scenario) {
            const BlockState liquid(scenario == 2 ? "minecraft:water" : scenario == 3
                                   ? "minecraft:lava" : "minecraft:air");
            for (int x = 64; x < 80; ++x)
                for (int z = 0; z < 16; ++z)
                    for (int y = 248; y <= 256; ++y)
                        level.setBlockState(x, y, z, y == 249 ? BlockState("minecraft:stone")
                                                                            : y >= 250 ? liquid : BlockState("minecraft:air"));
            ServerPlayer immediate(id, 4001 + scenario * 2);
            ServerPlayer delayed(id, 4002 + scenario * 2);
            for (ServerPlayer *player: {&immediate, &delayed}) {
                player->setLoginState(ServerPlayer::LoginState::Spawned);
                // Like a fresh login, keep lava damage out of the movement replay comparison.
                player->grantSpawnInvulnerability();
                player->getAttributes() = ActorAttributes::createPlayerDefaults();
                player->setPosition(Vector3f(70.5f, 250.0f, 2.5f));
                player->setOnGround(scenario < 2);
                if (scenario == 4) {
                    player->setGameType((int32_t) GameType::Creative);
                    player->setFlying(true);
                }
            }
            const auto packetFor = [&](int64_t tick) {
                auto packet = input(tick, 70.5f);
                packet.mPosition.z = 2.5f;
                packet.mMotionY = 1.0f;
                packet.mRotation.x = tick < 12 ? -30.0f : 30.0f;
                if (scenario == 1) {
                    packet.mInputData.push_back((int32_t) PlayerAuthInputData::Sprinting);
                    if (tick == 1 || tick == 14)
                        packet.mInputData.push_back((int32_t) PlayerAuthInputData::StartJumping);
                }
                if (scenario == 2 && tick == 1)
                    packet.mInputData.push_back((int32_t) PlayerAuthInputData::StartSwimming);
                if (scenario == 2 && tick >= 7 && tick <= 15)
                    packet.mInputData.push_back((int32_t) PlayerAuthInputData::Jumping);
                if (scenario == 3 && tick % 2 == 0)
                    packet.mInputData.push_back((int32_t) PlayerAuthInputData::Jumping);
                if (scenario == 4) {
                    packet.mMotionY = tick <= 8 || tick > 16 ? 1.0f : 0.0f;
                    if (tick <= 12)
                        packet.mInputData.push_back((int32_t) PlayerAuthInputData::WantUp);
                    if (tick > 18)
                        packet.mInputData.push_back((int32_t) PlayerAuthInputData::WantDown);
                }
                return packet;
            };
            for (int64_t tick = 1; tick <= 24; ++tick) {
                immediate.getMovementInputs().enqueue(packetFor(tick));
                MovementInputProbe::step(owner, immediate, tick);
                if (tick < 24)
                    MovementInputProbe::step(owner, delayed, tick);
            }
            for (int64_t tick = 1; tick <= 24; ++tick)
                delayed.getMovementInputs().enqueue(packetFor(tick));
            MovementInputProbe::step(owner, delayed, 24, 1000);
            const Vector3f positionDelta(immediate.getSimulatedPosition().x - delayed.getSimulatedPosition().x,
                                         immediate.getSimulatedPosition().y - delayed.getSimulatedPosition().y,
                                         immediate.getSimulatedPosition().z - delayed.getSimulatedPosition().z);
            const Vector3f velocityDelta(immediate.getSimulatedVelocity().x - delayed.getSimulatedVelocity().x,
                                         immediate.getSimulatedVelocity().y - delayed.getSimulatedVelocity().y,
                                         immediate.getSimulatedVelocity().z - delayed.getSimulatedVelocity().z);
            require(immediate.hasSimulatedPosition() && delayed.hasSimulatedPosition()
                    && std::fabs(positionDelta.x) < 0.000001f && std::fabs(positionDelta.y) < 0.000001f
                    && std::fabs(positionDelta.z) < 0.000001f && std::fabs(velocityDelta.x) < 0.000001f
                    && std::fabs(velocityDelta.y) < 0.000001f && std::fabs(velocityDelta.z) < 0.000001f,
                    ("Ground/jump/water/lava/flight catch-up diverged in scenario " + std::to_string(scenario)).c_str());
            require(immediate.isOnGround() == delayed.isOnGround()
                    && immediate.getJumpDelay() == delayed.getJumpDelay()
                    && delayed.getMovementInputs().lastProcessedTick() == 24,
                    "Catch-up lost collision, jump cooldown or input timeline state");
        }
    }

    void checkServerPipeline() {
        ServerNetworkHandler owner("Input regression", "", 2);
        const auto path = ServerPaths::getRoot() / "timeline.properties";
        std::ofstream(path) << "simulation=false\n";
        owner.setProperties(PropertiesSettings(path.string()));
        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 1001).first->second;
        player.setLoginState(ServerPlayer::LoginState::Spawned);
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
            const float expectedFeedback = settings.getServerAuthoritativeMovementStrict() ? 0.0f : 0.005f;
            require(feet.x == 66.5f,
                    "Position reconciliation moved the physical result before the next tick");
            require(std::fabs(player.getSimulatedVelocity().x - expectedFeedback) < 0.00001f
                    && player.getMotion().x == player.getSimulatedVelocity().x,
                    "Velocity feedback ignored strict mode or trusted the client's delta");
            player.setSimulatedPosition(player.getPosition());
            player.setSimulatedVelocity(Vector3f(0, -0.0784f, 0));
            feet = Vector3f(66.502f, 250.0f, 2.5f);
            packet.mTick = 200;
            PlayerMovementSimulator::apply(owner, id, player, packet, feet, false);
            require(feet.x == 66.5f
                    && std::fabs(player.getSimulatedVelocity().x - (66.502f - 66.5f)) < 0.00001f
                    && player.getMotion().x == player.getSimulatedVelocity().x,
                    "Fine acceptance did not steer velocity independently of client delta");
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
                              float forward = 1.0f, float pitch = 0.0f) {
            PlayerAuthInputPacket packet;
            packet.mMotionY = forward;
            packet.mRotation.x = pitch;
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

        auto leavingSwim = makePlayer();
        leavingSwim->getSwimAmount().mCurrent = 0.5f;
        step(*leavingSwim, {(int32_t) PlayerAuthInputData::Jumping}, false, 0.0f);
        close(leavingSwim->getPosition().y, 250.0f,
              "Ground jump ignored the remaining swim pose transition");
        leavingSwim->getSwimAmount().mCurrent = 0.0f;
        step(*leavingSwim, {(int32_t) PlayerAuthInputData::Jumping}, false, 0.0f);
        close(leavingSwim->getPosition().y, 250.42f, "Ground jump remained blocked after the swim pose ended");

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
        // BDS LiquidPhysicsSystem 0x140FC98E0 contracts the sensing volume, independently of eye water.
        const Vector3f shallowFeet(50.5f, 250.75f, 2.5f);
        for (int x = 49; x <= 52; ++x)
            for (int z = 1; z <= 4; ++z)
                for (int y = 251; y <= 252; ++y)
                    level.setBlockState(x, y, z, air);
        require(!LiquidBlocksFetch::at(level, shallowFeet).water
                && LiquidBlocksFetch::at(level, Vector3f(50.5f, 250.59f, 2.5f)).water,
                "Shallow water still used the player's uncontracted collision box");
        auto shallow = makePlayer(false);
        shallow->setPosition(shallowFeet);
        shallow->setSimulatedPosition(shallowFeet);
        step(*shallow);
        close(shallow->getSimulatedVelocity().z, 0.017836f,
              "Player above shallow water received underwater acceleration and friction");
        close(shallow->getSimulatedVelocity().y, -0.0784f,
              "Player above shallow water received water gravity");
        terrain(stone, air);
        level.setBlockState(51, 250, 2, WaterBlock::source());
        require(!LiquidBlocksFetch::at(level, Vector3f(50.7f, 250.0f, 2.5f)).water
                && LiquidBlocksFetch::at(level, Vector3f(50.71f, 250.0f, 2.5f)).water,
                "Touching a water block boundary was treated as water travel");
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
        // BDS SwimControlSystem 0x140F4FB60 and MobJumpSystem 0x140EB1110.
        terrain(air, WaterBlock::source());
        const std::vector<int32_t> swimFlags{(int32_t) PlayerAuthInputData::StartSwimming};
        for (bool sprinting: {false, true}) {
            auto dive = makePlayer(false);
            step(*dive, swimFlags, sprinting, 0.0f, 90.0f);
            close(dive->getPosition().y, 249.915f, "Swim steering did not precede collision movement");
            close(dive->getSimulatedVelocity().y, -0.068f, "BDS downward swim steering or drag differs");
            auto ascend = makePlayer(false);
            step(*ascend, swimFlags, sprinting, 0.0f, -90.0f);
            close(ascend->getSimulatedVelocity().y, 0.048f, "BDS upward swim steering or drag differs");
        }
        auto shallowDive = makePlayer(false);
        step(*shallowDive, swimFlags, false, 0.0f, 10.0f);
        require(shallowDive->getSimulatedVelocity().y < -0.008f
                && shallowDive->getSimulatedVelocity().y > -0.009f,
                "Shallow diving used the steep-dive steering coefficient");
        auto swimmingJump = makePlayer(false);
        step(*swimmingJump, {(int32_t) PlayerAuthInputData::StartSwimming,
                             (int32_t) PlayerAuthInputData::Jumping}, false, 0.0f, 90.0f);
        close(swimmingJump->getSimulatedVelocity().y, 0.032f,
              "Swim steering ran while MobIsJumping should exclude it");
        // Recorded ticks 560..563 hold Y still until the ten-tick pose transition completes.
        auto transition = makePlayer(false);
        step(*transition, {(int32_t) PlayerAuthInputData::StartSwimming}, false, 0.0f);
        transition->getFlags().set(ActorFlag::Swimming, true);
        for (int tick = 2; tick <= 10; ++tick) {
            step(*transition, {(int32_t) PlayerAuthInputData::Jumping}, false, 0.0f);
            close(transition->getSimulatedVelocity().y, 0.0f,
                  "Water jump accelerated during the BDS swim transition");
            close(transition->getPosition().y, 250.0f,
                  "Swim transition accumulated the recorded vertical drift");
        }
        close(transition->getSwimAmount().mCurrent, 1.0f, "Swim transition did not finish in ten inputs");
        step(*transition, {(int32_t) PlayerAuthInputData::Jumping}, false, 0.0f);
        close(transition->getSimulatedVelocity().y, 0.032f,
              "Completed swim transition kept suppressing the water jump");
        step(*transition, {(int32_t) PlayerAuthInputData::StopSwimming}, false, 0.0f);
        transition->getFlags().set(ActorFlag::Swimming, false);
        step(*transition, {(int32_t) PlayerAuthInputData::Jumping}, false, 0.0f);
        close(transition->getSimulatedVelocity().y, -0.005f,
              "Leaving the swim pose failed to clamp the water jump before sinking gravity");
        transition->clearMovementSimulation();
        close(transition->getSwimAmount().mCurrent, 0.8f,
              "Prediction reset restarted the independent swim pose transition");
        auto swimmingSink = makePlayer(false);
        step(*swimmingSink, {(int32_t) PlayerAuthInputData::StartSwimming,
                             (int32_t) PlayerAuthInputData::WantDownSlow}, false, 0.0f);
        close(swimmingSink->getSimulatedVelocity().y, -0.03008f,
              "Water sink input and swim steering order differs");
        auto waterJump = makePlayer(false);
        step(*waterJump, {(int32_t) PlayerAuthInputData::Jumping}, false, 0.0f);
        close(waterJump->getSimulatedVelocity().y, 0.027f, "Standing water jump or sinking gravity differs");
        auto waterSink = makePlayer(false);
        step(*waterSink, {(int32_t) PlayerAuthInputData::WantDownSlow}, false, 0.0f);
        close(waterSink->getSimulatedVelocity().y, -0.037f, "Standing water sink or drag differs");

        terrain(air, air);
        level.setBlockState(50, 251, 2, WaterBlock::source());
        const Vector3f waterBelowCeiling(50.5f, 250.0f, 2.5f);
        require(LiquidBlocksFetch::at(level, Vector3f(50.5f, 249.7f, 2.5f)).water
                && !LiquidBlocksFetch::at(level, Vector3f(50.5f, 249.7f, 2.5f), false, 1.49f).water,
                "Liquid sensing ignored the crouching collision height");
        require(LiquidBlocksFetch::at(level, waterBelowCeiling).water
                && !LiquidBlocksFetch::at(level, waterBelowCeiling, true).water,
                "Horizontal liquid sampling still included the standing body volume");
        terrain(stone, air);
        for (int x = 49; x <= 52; ++x)
            for (int z = 1; z <= 4; ++z)
                level.setBlockState(x, 250, z, WaterBlock::source());
        const LiquidContact horizontalWater = LiquidBlocksFetch::at(level, waterBelowCeiling, true);
        require(horizontalWater.eyeInWater && horizontalWater.swimProbeInWater
                && !LiquidBlocksFetch::at(level, waterBelowCeiling).eyeInWater,
                "Swimming eyes or swim-control probe used standing offsets");
        const LiquidContact nearSurface = LiquidBlocksFetch::at(level, Vector3f(50.5f, 250.55f, 2.5f), true);
        require(nearSurface.eyeInWater && nearSurface.swimProbeInWater,
                "Swim steering used a body-height probe above the head block");
        const LiquidContact crouched = LiquidBlocksFetch::at(level, Vector3f(50.5f, 249.5f, 2.5f), false, 1.49f);
        require(crouched.eyeInWater,
                "Crouched water sensing kept the standing eye height");
        auto surface = makePlayer(false);
        surface->setPosition(Vector3f(50.5f, 250.65f, 2.5f));
        surface->setSimulatedPosition(surface->getPosition());
        surface->setSimulatedVelocity(Vector3f(0, 0.2f, 0));
        step(*surface, swimFlags, false, 0.0f, -90.0f);
        close(surface->getSimulatedVelocity().y, 0.0f, "Looking up accelerated a swimmer out of the water");
        auto surfaceJump = makePlayer(false);
        surfaceJump->setPosition(Vector3f(50.5f, 250.65f, 2.5f));
        surfaceJump->setSimulatedPosition(surfaceJump->getPosition());
        surfaceJump->setSimulatedVelocity(Vector3f(0, 0.2f, 0));
        step(*surfaceJump, {(int32_t) PlayerAuthInputData::StartSwimming,
                           (int32_t) PlayerAuthInputData::Jumping}, false, 0.0f);
        close(surfaceJump->getSimulatedVelocity().y, 0.0f, "Swimming jump escaped the head-out-water clamp");
        // A low tunnel must not collide with the standing head while swimming.
        terrain(stone, WaterBlock::source());
        level.setBlockState(50, 251, 2, stone);
        auto tunnel = makePlayer(false);
        step(*tunnel, swimFlags, false, 0.0f, -90.0f);
        close(tunnel->getPosition().y, 250.06f, "Swimming retained a standing collision box");

        for (bool lavaCurrent: {false, true}) {
            terrain(air, air);
            const BlockState falling = lavaCurrent
                                       ? LavaBlock(BlockState("minecraft:lava")).makeState(0, true)
                                       : WaterBlock(WaterBlock::source()).makeState(0, true);
            for (int y = 250; y <= 252; ++y) {
                level.setBlockState(50, y, 2, falling);
                level.setBlockState(51, y, 2, stone);
            }
            const LiquidContact current = LiquidBlocksFetch::at(level, waterBelowCeiling);
            close(current.flow.y, -1.0f, "Liquid current was not normalized across multiple blocks");
            close(LiquidBlocksFetch::at(level, waterBelowCeiling, true).flow.y, -1.0f,
                  "Current strength depended on the player's pose");
            auto waterfall = makePlayer(false);
            step(*waterfall, {}, false, 0.0f);
            close(waterfall->getSimulatedVelocity().y, lavaCurrent ? -0.02175f : -0.0162f,
                  "BDS vertical liquid current strength or integration differs");
        }
        terrain(air, air);
        level.setBlockState(50, 250, 2, WaterBlock::source());
        level.setBlockState(50, 251, 2, LavaBlock(BlockState("minecraft:lava")).makeState(0));
        auto mixedLiquids = makePlayer(false);
        step(*mixedLiquids, {}, false, 0.0f);
        close(mixedLiquids->getSimulatedVelocity().y, -0.02f, "Water overrode BDS lava travel priority");

        terrain(stone, WaterBlock::source());
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
        close(levitating->getSimulatedVelocity().y, 0.0196f, "BDS levitation integration differs");
        levitating->setMotion(levitating->getSimulatedVelocity());
        levitating->tickEffects();
        close(levitating->getMotion().y, 0.0196f, "Levitation was integrated twice in one server tick");

        auto flying = makePlayer(false);
        flying->setGameType((int32_t) GameType::Creative);
        flying->setFlying(true);
        step(*flying, {(int32_t) PlayerAuthInputData::WantUp}, true);
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
        checkSprintAttributeSynchronization();
        checkPositionReconciliation();
        checkPredictionVelocityFeedback();
        checkCorrectionDiagnostics();
        checkServerPipeline();
        checkSimulationSettings();
        checkTravelPhysics();
        checkTravelReplay();
        checkGroundJumpTrajectory();
        checkShallowWaterWalking();
        checkCreativeFlightTrajectory();
        checkAnalogInputSlowdown();
        checkConfinedSwimmingPose();
        checkClientAcceptance();
        checkMovementTrace();
        checkLoadingScreenMovement();
        checkQueuedFlightRequests();
        checkFlightInputTransitions();
        checkShallowWaterExit();
    } catch (const std::exception &error) {
        std::clog << "Movement input regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
