#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/AI/Goal/OcelotSitOnBlockGoal.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/ChestBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    const Vector3i CHEST(3, 100, 0);

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"OcelotSit", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getOverworld();

        Arena() {
            require(mLevel.openStorage(ServerPaths::getRoot().string()), "could not open ocelot sit storage");
            for (int x = -1; x <= 1; ++x) {
                for (int z = -1; z <= 1; ++z)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            for (int x = -10; x <= 10; ++x) {
                for (int z = -10; z <= 10; ++z)
                    mLevel.setBlockState(x, 99, z, VanillaBlocks::STONE().toBlockState());
            }
        }

        ServerPlayer &player(uint64_t runtimeId) {
            const NetworkIdentifier id(std::to_string(runtimeId), runtimeId);
            auto &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(mLevel);
            player.setPosition(Vector3f(-5.5f, 100.0f, 0.5f));
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            player.setGameType((int32_t) GameType::Survival);
            return player;
        }

        MobActor &spawnCat(const Vector3f &position) {
            auto *cat = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, "minecraft:cat", position,
                    [](ServerActor &actor) {
                        static_cast<MobActor &>(actor).setSpawnEvent("minecraft:spawn_wild_adult");
                    }));
            require(cat != nullptr, "the cat did not spawn");
            cat->startDefinition(mOwner);
            return *cat;
        }

        bool seats(const char *identifier, const Vector3i &position, const BlockState &state) {
            const Block *block = VanillaBlocks::fromIdentifier(identifier);
            require(block != nullptr, std::string("no block class for ") + identifier);
            return block->seatsPet(mLevel, position, state);
        }
    };

    BlockState bedPart(bool head) {
        BlockState state(BlockNames::BED);
        state.mStates.putInt("direction", 0);
        state.mStates.putByte("head_piece_bit", head ? 1 : 0);
        state.mStates.putByte("occupied_bit", 0);
        return state;
    }

    BaseMoveToBlockGoal::Settings sitSettings(const MobActor &cat) {
        BaseMoveToBlockGoal::Settings settings;
        settings.mSpeed = cat.getMovementSpeed();
        settings.mSearchRange = 8;
        settings.mSearchHeight = 1;
        settings.mGoalRadius = 1.0f;
        return settings;
    }

    // a closed chest, a lit furnace and the foot of a bed; nothing else
    void seats() {
        Arena arena;
        const Vector3i at(0, 100, 0);
        require(arena.seats(BlockNames::CHEST, at, BlockState(BlockNames::CHEST)), "a chest is no seat");
        require(!arena.seats(BlockNames::TRAPPED_CHEST, at, BlockState(BlockNames::TRAPPED_CHEST)),
                "a trapped chest is a seat");
        require(!arena.seats(BlockNames::FURNACE, at, BlockState(BlockNames::FURNACE)), "an unlit furnace is a seat");
        require(arena.seats(BlockNames::LIT_FURNACE, at, BlockState(BlockNames::LIT_FURNACE)),
                "a lit furnace is no seat");
        require(!arena.seats(BlockNames::LIT_BLAST_FURNACE, at, BlockState(BlockNames::LIT_BLAST_FURNACE)),
                "a lit blast furnace is a seat");
        require(!arena.seats(BlockNames::LIT_SMOKER, at, BlockState(BlockNames::LIT_SMOKER)), "a lit smoker is a seat");
        require(arena.seats(BlockNames::BED, at, bedPart(false)), "the foot of a bed is no seat");
        require(!arena.seats(BlockNames::BED, at, bedPart(true)), "the head of a bed is a seat");
        require(!arena.seats(BlockNames::STONE, at, VanillaBlocks::STONE().toBlockState()), "stone is a seat");

        arena.mLevel.setBlockState(at.x, at.y, at.z, BlockState(BlockNames::CHEST));
        ChestBlockActor &chest = ChestBlock::getOrCreate(arena.mLevel, at);
        chest.addViewer();
        require(!arena.seats(BlockNames::CHEST, at, BlockState(BlockNames::CHEST)), "an open chest is a seat");
    }

    // a tamed cat walks to the chest, sits on it and gets up once someone opens it
    void catSitsOnChest() {
        Arena arena;
        ServerPlayer &player = arena.player(1);
        arena.mLevel.setBlockState(CHEST.x, CHEST.y, CHEST.z, BlockState(BlockNames::CHEST));
        MobActor &wild = arena.spawnCat(Vector3f(0.5f, 100.0f, 0.5f));
        OcelotSitOnBlockGoal wildGoal(sitSettings(wild));
        require(!wildGoal.canUse(arena.mOwner, wild), "a wild cat sat on a chest");

        MobActor &cat = arena.spawnCat(Vector3f(0.5f, 100.0f, 0.5f));
        require(cat.tameTo(arena.mOwner, player, "minecraft:tameable", false), "the cat was not tamed");
        OcelotSitOnBlockGoal goal(sitSettings(cat));
        require(goal.canUse(arena.mOwner, cat), "the tamed cat found no chest to sit on");
        goal.start(arena.mOwner, cat);
        require(!cat.getNavigation().isDone(), "the cat did not walk to the chest");

        goal.tick(arena.mOwner, cat);
        require(!cat.getFlags().get(ActorFlag::Sitting), "the cat sat down before reaching the chest");

        cat.setPosition(Vector3f(3.5f, 100.875f, 0.5f));
        goal.tick(arena.mOwner, cat);
        require(cat.getFlags().get(ActorFlag::Sitting), "the cat did not sit on the chest");
        require(!cat.isSitting(), "sitting on the chest told the cat to stay");
        require(goal.canContinueToUse(arena.mOwner, cat), "the cat left the chest at once");

        ChestBlock::getOrCreate(arena.mLevel, CHEST).addViewer();
        require(!goal.canContinueToUse(arena.mOwner, cat), "the cat stayed on an open chest");
        goal.stop(arena.mOwner, cat);
        require(!cat.getFlags().get(ActorFlag::Sitting), "the cat kept sitting after the chest opened");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-ocelot-sit-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        seats();
        catSitsOnChest();
        std::cout << "ocelot sit on block checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
