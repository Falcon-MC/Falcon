#include "Core/Debug/BedrockLog.h"
#include "Core/Debug/ContentLogEndPoint.h"
#include "Core/Math/Vector3f.h"
#include "Network/Handler/ChunkStreamHandler.h"
#include "Network/Handler/ChatHandler.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/MovePlayerPacket.h"
#include "Protocol/Packets/SetActorMotionPacket.h"
#include "Protocol/Packets/UpdateBlockPacket.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // the chunk offsets the game copies into each level
    const std::pair<int32_t, int32_t> GAME_OFFSETS[] = {
            {-1, -4}, {0, -4}, {1, -4}, {-2, -3}, {-1, -3}, {0, -3},  {1, -3},  {2, -3},  {-3, -2}, {-2, -2},
            {-1, -2}, {0, -2}, {1, -2}, {2, -2},  {3, -2},  {-4, -1}, {-3, -1}, {-2, -1}, {-1, -1}, {0, -1},
            {1, -1},  {2, -1}, {3, -1}, {4, -1},  {-4, 0},  {-3, 0},  {-2, 0},  {-1, 0},  {0, 0},   {1, 0},
            {2, 0},   {3, 0},  {4, 0},  {-4, 1},  {-3, 1},  {-2, 1},  {-1, 1},  {0, 1},   {1, 1},   {2, 1},
            {3, 1},   {4, 1},  {-3, 2}, {-2, 2},  {-1, 2},  {0, 2},   {1, 2},   {2, 2},   {3, 2},   {-2, 3},
            {-1, 3},  {0, 3},  {1, 3},  {2, 3},   {-1, 4},  {0, 4},   {1, 4},
    };

    Vector3f inChunk(int32_t chunkX, int32_t chunkZ) {
        return Vector3f((float) chunkX * 16.0f + 8.0f, 64.0f, (float) chunkZ * 16.0f + 8.0f);
    }

    void movementRecipients() {
        ServerNetworkHandler owner("Visibility", "", 1);
        Level &level = owner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
        level.setOwner(&owner);
        level.insertChunk(LevelChunk(0, 0)).setPopulated(true);
        std::vector<ServerPlayer *> players;
        for (uint64_t i = 0; i < 4; ++i) {
            const NetworkIdentifier id("visibility", 100 + i);
            ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 100 + i).first->second;
            player.moveToLevel(level);
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            player.setPosition(Vector3f(1.0f + (float) i, 64.0f, 1.0f));
            players.push_back(&player);
        }
        ServerPlayer &mover = *players[0];
        players[1]->getVisiblePlayers().insert(mover.getRuntimeId());
        players[2]->getVisiblePlayers().insert(mover.getRuntimeId());
        std::vector<NetworkIdentifier> recipients;
        owner.getNetworkHandler().setOutboundFilter([&](const NetworkIdentifier &id, std::string &data) {
            ReadOnlyBinaryStream stream(data);
            MovePlayerPacket packet;
            packet.readHeader(stream);
            packet.read(stream, owner.getCodecContext());
            require(packet.mRuntimeActorId == (int64_t) mover.getRuntimeId(), "movement actor changed");
            require(packet.mPosition.x == mover.getPosition().x, "movement position changed");
            recipients.push_back(id);
            return false;
        });
        mover.markMoveBroadcast();
        mover.setPosition(Vector3f(2.0f, 64.0f, 1.0f));
        owner.broadcastPlayerMove(mover);
        owner.broadcastPlayerMove(mover);
        owner.getNetworkHandler().setOutboundFilter(nullptr);
        require(recipients.size() == 2, "movement must reach both visible remote viewers once");
        for (const NetworkIdentifier &id: recipients)
            require(id == players[1]->getNetworkIdentifier() || id == players[2]->getNetworkIdentifier(),
                    "hidden player received movement");
        require(recipients[0] != recipients[1], "movement was duplicated for one viewer");

        size_t hooks = 0;
        owner.getNetworkHandler().setSendHook([&](const std::vector<NetworkIdentifier> &ids, MinecraftPacketIds packet) {
            require(ids.size() == 1, "movement hook recipients were grouped");
            require(packet == MinecraftPacketIds::MovePlayer, "unexpected movement hook packet");
            ++hooks;
            return ids[0] == players[1]->getNetworkIdentifier();
        });
        recipients.clear();
        owner.getNetworkHandler().setOutboundFilter([&](const NetworkIdentifier &id, std::string &) {
            recipients.push_back(id);
            return false;
        });
        mover.setPosition(Vector3f(3.0f, 64.0f, 1.0f));
        owner.broadcastPlayerMove(mover);
        owner.getNetworkHandler().setOutboundFilter(nullptr);
        owner.getNetworkHandler().setSendHook(nullptr);
        require(hooks == 2, "movement hook did not run once per visible viewer");
        require(recipients.size() == 1 && recipients[0] == players[1]->getNetworkIdentifier(),
                "per-viewer movement cancellation changed");

        const auto checkBroadcast = [&](const std::function<void()> &send, size_t expected) {
            recipients.clear();
            owner.getNetworkHandler().setOutboundFilter([&](const NetworkIdentifier &id, std::string &) {
                recipients.push_back(id);
                return false;
            });
            send();
            require(recipients.size() == expected, "broadcast recipients changed");
            hooks = 0;
            owner.getNetworkHandler().setSendHook([&](const std::vector<NetworkIdentifier> &ids,
                                                      MinecraftPacketIds) {
                require(ids.size() == 1, "broadcast hook recipients were grouped");
                ++hooks;
                return ids[0] == players[1]->getNetworkIdentifier();
            });
            recipients.clear();
            send();
            require(hooks == expected && recipients.size() == 1
                            && recipients[0] == players[1]->getNetworkIdentifier(),
                    "per-viewer broadcast cancellation changed");
            owner.getNetworkHandler().setOutboundFilter(nullptr);
            owner.getNetworkHandler().setSendHook(nullptr);
        };
        checkBroadcast([&]() { mover.attackActor(owner, 0, nullptr, false); }, 2);
        checkBroadcast([&]() { ChatHandler::broadcastChat(owner, mover, "visibility test"); }, 4);
        SetActorMotionPacket motion;
        motion.mRuntimeActorId = mover.getRuntimeId();
        checkBroadcast([&]() { owner.sendToActorViewers(mover, motion, true); }, 3);

        ServerActor knownActor(500, "minecraft:zombie");
        knownActor.moveToLevel(level);
        knownActor.setPosition(Vector3f(2000.0f, 64.0f, 2000.0f));
        players[1]->getVisibleActors().insert(knownActor.getRuntimeId());
        motion.mRuntimeActorId = knownActor.getRuntimeId();
        checkBroadcast([&]() { owner.sendToActorViewers(knownActor, motion, false); }, 1);

        players[1]->getSentChunks().insert(0);
        players[2]->getSentChunks().insert(0);
        UpdateBlockPacket update;
        checkBroadcast([&]() {
            BlockActionHandler::broadcastToViewers(owner, level, mover.getPosition(), update);
        }, 2);
    }

    void selfDeliveryAfterTeleport() {
        ServerNetworkHandler owner("Self delivery", "", 1);
        Level &level = owner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
        level.setOwner(&owner);
        level.insertChunk(LevelChunk(0, 0)).setPopulated(true);
        const NetworkIdentifier moverId("mover", 100);
        const NetworkIdentifier viewerId("viewer", 101);
        ServerPlayer &mover = owner.getPlayers().try_emplace(moverId, moverId, 100).first->second;
        ServerPlayer &viewer = owner.getPlayers().try_emplace(viewerId, viewerId, 101).first->second;
        for (ServerPlayer *player: {&mover, &viewer}) {
            player->moveToLevel(level);
            player->setLoginState(ServerPlayer::LoginState::Spawned);
            player->setPosition(Vector3f(1.5f, 64.0f, 1.5f));
        }
        viewer.getVisiblePlayers().insert(mover.getRuntimeId());

        std::vector<NetworkIdentifier> recipients;
        owner.getNetworkHandler().setOutboundFilter([&](const NetworkIdentifier &id, std::string &) {
            recipients.push_back(id);
            return false;
        });
        SetActorMotionPacket motion;
        motion.mRuntimeActorId = mover.getRuntimeId();
        owner.sendToActorViewers(mover, motion, true);
        require(recipients.size() == 2, "player and viewer did not both receive the packet before the teleport");

        // the index stays where the tick built it: the teleport lands far outside the viewer walk
        mover.setPosition(Vector3f(2000.0f, 64.0f, 2000.0f));
        recipients.clear();
        owner.sendToActorViewers(mover, motion, true);
        require(recipients.size() == 1 && recipients[0] == moverId, "teleported player lost its own packet");

        size_t hooks = 0;
        owner.getNetworkHandler().setSendHook([&](const std::vector<NetworkIdentifier> &ids, MinecraftPacketIds) {
            require(ids.size() == 1 && ids[0] == moverId, "self hook reached another player");
            ++hooks;
            return true;
        });
        recipients.clear();
        owner.sendToActorViewers(mover, motion, true);
        require(hooks == 1 && recipients.size() == 1 && recipients[0] == moverId,
                "teleported player lost its own packet behind a send hook");
        owner.getNetworkHandler().setSendHook(nullptr);

        recipients.clear();
        owner.sendToActorViewers(mover, motion, false);
        require(recipients.empty(), "packet without self reached the player");
        owner.getNetworkHandler().setOutboundFilter(nullptr);
    }

    void visibilityTransitions() {
        ServerNetworkHandler owner("Visibility transitions", "", 1);
        Level &level = owner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
        level.setOwner(&owner);
        level.insertChunk(LevelChunk(0, 0)).setPopulated(true);
        level.insertChunk(LevelChunk(1, 0)).setPopulated(true);
        LevelChunk &farChunk = level.insertChunk(LevelChunk(5, 0));
        farChunk.setPopulated(true);
        const NetworkIdentifier viewerId("viewer", 100);
        const NetworkIdentifier targetId("target", 101);
        ServerPlayer &viewer = owner.getPlayers().try_emplace(viewerId, viewerId, 100).first->second;
        ServerPlayer &target = owner.getPlayers().try_emplace(targetId, targetId, 101).first->second;
        for (ServerPlayer *player: {&viewer, &target}) {
            player->getAttributes() = ActorAttributes::createPlayerDefaults();
            player->moveToLevel(level);
            player->setLoginState(ServerPlayer::LoginState::Spawned);
            player->setPosition(Vector3f(1.5f, 64.0f, 1.5f));
            player->setUuid(std::to_string(player->getRuntimeId()));
        }
        size_t packets = 0;
        owner.getNetworkHandler().setOutboundFilter([&](const NetworkIdentifier &, std::string &) {
            ++packets;
            return false;
        });
        const auto visible = [&]() { return viewer.getVisiblePlayers().count(101) != 0; };
        owner.updatePlayerVisibility();
        require(visible(), "nearby player was not spawned");
        const size_t firstPackets = packets;
        owner.updatePlayerVisibility();
        target.setPosition(Vector3f(3.5f, 64.0f, 1.5f));
        owner.updatePlayerVisibility();
        require(visible() && packets == firstPackets, "movement within a chunk changed visibility");

        target.setPosition(Vector3f(80.5f, 64.0f, 1.5f));
        owner.updatePlayerVisibility();
        require(!visible(), "target leaving range remained visible");
        viewer.setPosition(Vector3f(17.5f, 64.0f, 1.5f));
        owner.updatePlayerVisibility();
        require(visible(), "viewer entering range did not spawn the target");
        owner.setPlayerVisibleTo(viewer, target, false);
        owner.updatePlayerVisibility();
        require(!visible(), "hidden target was respawned");
        owner.setPlayerVisibleTo(viewer, target, true);
        owner.updatePlayerVisibility();
        require(visible(), "shown target remained hidden");

        farChunk.setPopulated(false);
        owner.updatePlayerVisibility();
        require(!visible(), "target in unpopulated chunk remained visible");
        farChunk.setPopulated(true);
        owner.updatePlayerVisibility();
        require(visible(), "target in repopulated chunk remained hidden");
        target.setLoginState(ServerPlayer::LoginState::StartGameSent);
        owner.updatePlayerVisibility();
        require(!visible(), "unspawned target remained visible");
        target.setLoginState(ServerPlayer::LoginState::Spawned);
        owner.updatePlayerVisibility();
        require(visible(), "respawned target remained hidden");
        owner.despawnPlayerFrom(viewer, target, true);
        owner.updatePlayerVisibility();
        require(visible(), "explicit despawn prevented the next visibility refresh");

        Level &nether = owner.getWorlds().getDefault().getLevel(DimensionType::Nether);
        nether.insertChunk(LevelChunk(5, 0)).setPopulated(true);
        target.moveToLevel(nether);
        owner.updatePlayerVisibility();
        require(!visible(), "target in another dimension remained visible");
        target.moveToLevel(level);
        owner.updatePlayerVisibility();
        require(visible(), "returning target remained hidden");
        owner.getPlayers().erase(targetId);
        owner.updatePlayerVisibility();
        require(!visible(), "disconnected target remained visible");
        owner.getNetworkHandler().setOutboundFilter(nullptr);
    }

    void gameOffsets() {
        const std::set<std::pair<int32_t, int32_t>> expected(std::begin(GAME_OFFSETS), std::end(GAME_OFFSETS));
        require(expected.size() == 57, "the game table holds 57 distinct chunks");

        for (const auto &centre: {std::make_pair(0, 0), std::make_pair(-3, 7), std::make_pair(1000, -1000)}) {
            for (int32_t dx = -8; dx <= 8; ++dx) {
                for (int32_t dz = -8; dz <= 8; ++dz) {
                    const bool seen = ChunkStreamHandler::isInActorRange(
                            inChunk(centre.first, centre.second), inChunk(centre.first + dx, centre.second + dz));
                    require(seen == (expected.count(std::make_pair(dx, dz)) != 0),
                            "an actor is sent exactly in the chunks of the game table");
                }
            }
        }
    }

    void viewDistanceIgnored() {
        // a mob 6 chunks away stays hidden however far the chunks stream
        require(!ChunkStreamHandler::isInActorRange(inChunk(0, 0), inChunk(6, 0)),
                "six chunks out is past the actor range");
        require(!ChunkStreamHandler::isInActorRange(inChunk(0, 0), inChunk(3, 3)),
                "the corner at 3, 3 is past the actor range");
        require(ChunkStreamHandler::isInActorRange(inChunk(0, 0), inChunk(4, 1)), "4, 1 is inside the actor range");
    }

    void negativeCoordinates() {
        // chunks come from floored positions: -0.5 is chunk -1, not 0
        require(ChunkStreamHandler::isInActorRange(Vector3f(-0.5f, 64.0f, 0.0f), Vector3f(-64.5f, 64.0f, 0.0f)),
                "-0.5 and -64.5 are four chunks apart");
        require(!ChunkStreamHandler::isInActorRange(Vector3f(0.5f, 64.0f, 0.0f), Vector3f(-64.5f, 64.0f, 0.0f)),
                "0.5 and -64.5 are five chunks apart");
        require(ChunkStreamHandler::isInActorRange(Vector3f(15.9f, 64.0f, -0.1f), Vector3f(-48.0f, 64.0f, -16.0f)),
                "the actor chunk is taken from its floored position");
    }
}

int main() {
    BedrockLog::addEndPoint(std::make_shared<ContentLogEndPoint>(false));

    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-visibility-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    ServerPaths::setRoot(directory);
    int failures = 0;
    const auto check = [&failures](const char *name, const std::function<void()> &run) {
        try {
            run();
            LOG_INFO(LogAreaID::Server, "PASS: %s", name);
        } catch (const std::exception &error) {
            ++failures;
            LOG_ERROR(LogAreaID::Server, "FAIL: %s: %s", name, error.what());
        }
    };

    check("actor range matches the game table", gameOffsets);
    check("actor range ignores the view distance", viewDistanceIgnored);
    check("actor range floors negative positions", negativeCoordinates);

    check("movement batching preserves remote recipients and per-viewer hooks", movementRecipients);
    check("visibility follows chunk, login, dimension and hide transitions", visibilityTransitions);
    check("a player keeps its own packets after a same-tick teleport", selfDeliveryAfterTeleport);
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    BedrockLog::flush();
    return failures == 0 ? 0 : 1;
}
