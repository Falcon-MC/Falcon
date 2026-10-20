#include "World/Actor/Movement/ServerPlayerMovementComponent.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
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
        checkServerPipeline();
    } catch (const std::exception &error) {
        std::clog << "Movement input regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
