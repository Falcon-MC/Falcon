#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/MoveActorAbsolutePacket.h"
#include "Server/Commands/ServerCommandOrigin.h"
#include "Server/Commands/SpreadPlayersCommand.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    class RecordingOrigin : public ServerCommandOrigin {
    public:
        explicit RecordingOrigin(ServerNetworkHandler *handler) : ServerCommandOrigin(handler) {}

        void sendTranslation(const std::string &key, const std::vector<std::string> &parameters) override {
            mLast = key;
            for (const std::string &parameter: parameters)
                mLast += "|" + parameter;
        }

        std::string mLast;
    };

    std::vector<std::string> split(const std::string &line) {
        std::vector<std::string> words;
        std::string word;
        for (const char c: line) {
            if (c == ' ') {
                if (!word.empty())
                    words.push_back(word);
                word.clear();
            } else {
                word += c;
            }
        }
        if (!word.empty())
            words.push_back(word);
        return words;
    }

    // a stone floor at y 63 over the chunks -2..3, as far as the cases reach
    struct Arena {
        ServerNetworkHandler mOwner{"Spread", "", 1};
        Level *mLevel = nullptr;
        RecordingOrigin mConsole{&mOwner};
        SpreadPlayersCommand mCommand{mOwner};
        std::vector<ServerPlayer *> mPlayers;

        Arena() {
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            mLevel->setOwner(&mOwner);
            for (int32_t x = -2; x <= 3; ++x) {
                for (int32_t z = -2; z <= 3; ++z)
                    mLevel->insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            floor(-32, 63, BlockState("minecraft:stone"));
            for (uint64_t index = 0; index < 3; ++index) {
                const NetworkIdentifier id("spread" + std::to_string(index), 7000 + index);
                ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, 7000 + index).first->second;
                player.setName("Spread" + std::to_string(index));
                player.getAttributes() = ActorAttributes::createPlayerDefaults();
                player.moveToLevel(*mLevel);
                player.setPosition(Vector3f(0.5f, 64.0f, 0.5f));
                player.setLoginState(ServerPlayer::LoginState::Spawned);
                mPlayers.push_back(&player);
            }
        }

        void floor(int32_t from, int32_t y, const BlockState &state) {
            for (int32_t x = from; x < 64; ++x) {
                for (int32_t z = from; z < 64; ++z)
                    mLevel->setBlockState(x, y, z, state);
            }
        }

        // the line as typed; the command gets the words after its name
        bool run(const std::string &line) {
            std::vector<std::string> words = split(line);
            words.erase(words.begin());
            return mCommand.execute(mConsole, words);
        }

        // a fixed list of numbers in [0, 1), read in a loop
        void randoms(std::vector<float> values) {
            size_t next = 0;
            mCommand.setRandom([values, next]() mutable { return values[next++ % values.size()]; });
        }
    };

    // three players land on the floor inside the square, at least the distance apart, and are counted
    void spreading(Arena &arena) {
        require(arena.run("spreadplayers 8 8 4 10 @a"), "the spread failed: " + arena.mConsole.mLast);
        require(arena.mConsole.mLast == "commands.spreadplayers.success.players|3|8.00|8.00",
                "the success did not count three players around 8, 8: " + arena.mConsole.mLast);
        for (size_t one = 0; one < arena.mPlayers.size(); ++one) {
            const Vector3f &at = arena.mPlayers[one]->getPosition();
            require(at.x >= -2.0f && at.x <= 19.0f && at.z >= -2.0f && at.z <= 19.0f, "a player landed outside the square");
            require(at.y == 65.0f, "a player did not land one block over the floor: " + std::to_string(at.y));
            require(at.x - std::floor(at.x) == 0.5f && at.z - std::floor(at.z) == 0.5f,
                    "a player was not put in the middle of a block");
            for (size_t other = one + 1; other < arena.mPlayers.size(); ++other) {
                const Vector3f &there = arena.mPlayers[other]->getPosition();
                const float apart = std::sqrt((at.x - there.x) * (at.x - there.x) + (at.z - there.z) * (at.z - there.z));
                require(apart >= 4.0f - 1.5f, "two players landed closer than the spread: " + std::to_string(apart));
            }
        }
    }

    void refusals(Arena &arena) {
        require(!arena.run("spreadplayers 0 0 -1 10 @a")
                        && arena.mConsole.mLast == "commands.generic.num.tooSmall|-1.00|0.00",
                "a negative spread was not refused: " + arena.mConsole.mLast);
        require(!arena.run("spreadplayers 0 0 5 5 @a")
                        && arena.mConsole.mLast == "commands.generic.num.tooSmall|5.00|6.00",
                "a range under the spread plus one was not refused: " + arena.mConsole.mLast);
        require(!arena.run("spreadplayers 0 0 1 10 @e[type=cow]")
                        && arena.mConsole.mLast == "commands.generic.noTargetMatch",
                "a spread of no one was not refused: " + arena.mConsole.mLast);
    }

    // ground that hurts or swallows is refused; plain stone is fine
    void ground(Arena &arena) {
        Level &level = *arena.mLevel;
        const int32_t top = level.getMaxY();
        require(SpreadPlayersCommand::isSafe(level, 40, 40, top), "plain stone was unsafe");
        for (const char *hazard: {"minecraft:magma", "minecraft:sweet_berry_bush", "minecraft:wither_rose",
                                  "minecraft:powder_snow", "minecraft:lava"}) {
            const int32_t y = std::string(hazard) == "minecraft:magma" || std::string(hazard) == "minecraft:lava" ? 63 : 64;
            const BlockState kept = level.getBlockState(41, y, 41);
            level.setBlockState(41, y, 41, BlockState(hazard));
            require(!SpreadPlayersCommand::isSafe(level, 41, 41, top), std::string("landing on ") + hazard + " was safe");
            level.setBlockState(41, y, 41, kept);
        }
        // water on top of the column, whatever lies under it
        level.setBlockState(42, 64, 42, BlockState("minecraft:water"));
        require(!SpreadPlayersCommand::isSafe(level, 42, 42, top), "a column of water was safe");
        level.setBlockState(42, 64, 42, BlockState("minecraft:air"));
        // liquid on top of a column counts even when the search starts under it
        for (int32_t y = 70; y <= 71; ++y)
            level.setBlockState(44, y, 44, BlockState(y == 70 ? "minecraft:stone" : "minecraft:water"));
        require(SpreadPlayersCommand::isSafe(level, 45, 45, 68), "a clear column under a height limit was unsafe");
        require(!SpreadPlayersCommand::isSafe(level, 44, 44, 68), "a column topped with water was safe");
        level.setBlockState(44, 70, 44, BlockState("minecraft:air"));
        level.setBlockState(44, 71, 44, BlockState("minecraft:air"));

        // a roof: without a maximum height the search starts on top of it
        level.setBlockState(43, 80, 43, BlockState("minecraft:stone"));
        require(SpreadPlayersCommand::landingY(level, 43, 43, top) == 82.0f, "the landing missed the roof");
        require(SpreadPlayersCommand::landingY(level, 43, 43, 70) == 65.0f, "the maximum height did not go under the roof");
        level.setBlockState(43, 80, 43, BlockState("minecraft:air"));
    }

    // a maximum height keeps everyone under the roof
    void underRoof(Arena &arena) {
        arena.floor(-32, 75, BlockState("minecraft:stone"));
        require(arena.run("spreadplayers 8 8 2 6 @a 70"), "the spread under a roof failed: " + arena.mConsole.mLast);
        for (const ServerPlayer *player: arena.mPlayers)
            require(player->getPosition().y == 65.0f, "a player landed on the roof");
        arena.floor(-32, 75, BlockState("minecraft:air"));
    }

    void entities(Arena &arena) {
        ServerActor *cow = arena.mOwner.spawnActor(*arena.mLevel, "minecraft:cow", Vector3f(-20.5f, 64.0f, -20.5f));
        require(cow != nullptr, "no cow spawned");
        // a player who sees the cow is told it jumped
        arena.mPlayers[0]->getVisibleActors().insert(cow->getRuntimeId());
        bool told = false;
        arena.mOwner.getNetworkHandler().setOutboundFilter([&](const NetworkIdentifier &, std::string &data) {
            ReadOnlyBinaryStream header(data);
            unsigned char sender = 0, receiver = 0;
            if (Packet::peekId(header, sender, receiver) == MinecraftPacketIds::MoveActorAbsolute) {
                ReadOnlyBinaryStream body(data);
                MoveActorAbsolutePacket packet;
                packet.readHeader(body);
                packet.read(body, arena.mOwner.getCodecContext());
                told = told || (packet.mRuntimeActorId == (int64_t) cow->getRuntimeId() && packet.mTeleported);
            }
            return false;
        });
        require(arena.run("spreadplayers 20 20 1 3 @e[type=cow]"), "the cow was not spread: " + arena.mConsole.mLast);
        arena.mOwner.getNetworkHandler().setOutboundFilter(nullptr);
        require(told, "the cow's jump was not sent to the player who sees it");
        const Vector3f &at = cow->getPosition();
        require(at.x >= 16.0f && at.x <= 24.0f && at.z >= 16.0f && at.z <= 24.0f && at.y == 65.0f,
                "the cow did not land around 20, 20");
    }

    // points too close step away from each other and stay inside the square; points on the same spot are thrown
    // again
    void pushing(Arena &arena) {
        arena.randoms({0.0f, 0.0f, 0.05f, 0.0f, 0.1f, 0.0f});
        require(arena.run("spreadplayers 8 8 3 4 @a"), "players near an edge were not spread: " + arena.mConsole.mLast);
        for (size_t one = 0; one < arena.mPlayers.size(); ++one) {
            const Vector3f &at = arena.mPlayers[one]->getPosition();
            require(at.x >= 4.0f && at.x <= 12.5f && at.z >= 4.0f && at.z <= 12.5f,
                    "a pushed player left the square: " + std::to_string(at.x) + ", " + std::to_string(at.z));
            for (size_t other = one + 1; other < arena.mPlayers.size(); ++other) {
                const Vector3f &there = arena.mPlayers[other]->getPosition();
                require(std::abs(at.x - there.x) + std::abs(at.z - there.z) >= 1.5f, "pushed players stayed together");
            }
        }

        arena.randoms({0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.1f, 0.9f, 0.9f, 0.1f, 0.5f, 0.2f});
        require(arena.run("spreadplayers 8 8 3 10 @a"), "players on one spot were not thrown apart: " + arena.mConsole.mLast);
    }

    // with nowhere safe the command gives up, naming the closest pair when there was one
    void failures(Arena &arena) {
        arena.floor(-32, 63, BlockState("minecraft:lava"));
        require(!arena.run("spreadplayers 8 8 1 4 @a[name=Spread0]"), "a spread over lava succeeded");
        require(arena.mConsole.mLast == "commands.spreadplayers.failure.players.unsafe|1|8.00|8.00|10000",
                "a lone player over lava was not unsafe: " + arena.mConsole.mLast);
        // the closest pair is the one of the last round: the throws alternate a near pair and a far one
        arena.randoms({0.5f, 0.5f, 0.5f, 0.75f, 0.0f, 0.0f, 1.0f, 1.0f});
        require(!arena.run("spreadplayers 8 8 1 4 @a[name=!Spread2]"), "a spread over lava succeeded");
        require(arena.mConsole.mLast == "commands.spreadplayers.failure.players|2|8.00|8.00|11.31",
                "the failure did not name the last round's closest pair: " + arena.mConsole.mLast);
        arena.floor(-32, 63, BlockState("minecraft:stone"));

        // every throw lands on the same point: the players never part
        arena.randoms({0.5f});
        require(!arena.run("spreadplayers 8 8 3 6 @a"), "players that never part were spread");
        require(arena.mConsole.mLast == "commands.spreadplayers.failure.players|3|8.00|8.00|0.00",
                "the failure did not name the closest pair: " + arena.mConsole.mLast);
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-spread-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        Arena arena;
        spreading(arena);
        refusals(arena);
        ground(arena);
        underRoof(arena);
        entities(arena);
        pushing(arena);
        failures(arena);
    } catch (const std::exception &error) {
        std::clog << "Spread players regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
