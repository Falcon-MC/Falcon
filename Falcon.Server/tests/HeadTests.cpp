#include "CircuitTicks.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/AI/Goal/NearestAttackableTargetGoal.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/PlayerInventory.h"
#include "Protocol/Types/ItemDefinitionRegistry.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/SkullBlockActor.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    ItemStack stack(const char *identifier) {
        ItemStack item;
        item.mDefinition = std::make_shared<ItemDefinition>(identifier, 1, false, Tag());
        item.mCount = 1;
        return item;
    }

    // the spawn chunk keeps its generated terrain: the cases stay in the empty chunks east of it
    struct Arena {
        ServerNetworkHandler mOwner{"Heads", "", 1};
        Level *mLevel = nullptr;

        explicit Arena(const std::filesystem::path &directory) {
            const std::filesystem::path path = directory / "hard.properties";
            std::ofstream(path) << "difficulty=hard\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            for (int32_t x = 1; x <= 4; x++) {
                for (int32_t z = -1; z <= 1; z++) {
                    mLevel->insertChunk(LevelChunk(x, z)).setPopulated(true);
                    mLevel->getBlockActors().loadChunk(x, z, {}, mOwner.getCodecContext());
                }
            }
        }

        BlockState block(const char *identifier) {
            const Block *value = VanillaBlocks::fromIdentifier(identifier);
            require(value != nullptr, std::string("no block ") + identifier);
            return value->toBlockState();
        }

        SkullBlockActor &head(const char *identifier, const Vector3i &position) {
            const BlockState state = block(identifier);
            mLevel->setBlockState(position.x, position.y, position.z, state);
            SkullBlockActor::place(mOwner, *mLevel, position, state, 0.0f);
            SkullBlockActor *skull = mLevel->getBlockActors().find<SkullBlockActor>(position);
            require(skull != nullptr, "the head has no block actor");
            return *skull;
        }

        void update(const Vector3i &position) {
            (void) position;
            settleCircuit(mOwner, *mLevel);
        }

        MobActor &mob(const char *identifier, const Vector3f &position) {
            auto *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(*mLevel, identifier, position));
            require(mob != nullptr, std::string("no ") + identifier);
            return *mob;
        }

        ServerPlayer &player(uint64_t runtimeId, const Vector3f &position) {
            const NetworkIdentifier id(std::to_string(runtimeId), runtimeId);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(*mLevel);
            player.setPosition(position);
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            return player;
        }
    };

    // a powered dragon head moves its mouth and counts the ticks; unpowered it stops
    void dragonMouth(Arena &arena) {
        const Vector3i at(24, 100, 0);
        SkullBlockActor &head = arena.head("minecraft:dragon_head", at);
        arena.update(at);
        require(!head.isDoingAnimation(), "an unpowered dragon head moves its mouth");

        arena.mLevel->setBlockState(at.x + 1, at.y, at.z, arena.block("minecraft:redstone_block"));
        arena.update(at);
        require(head.isDoingAnimation(), "a powered dragon head does not move its mouth");
        for (int32_t tick = 0; tick < 5; ++tick)
            arena.mLevel->getBlockActors().tick(arena.mOwner);
        require(head.getMouthTickCount() == 5, "the mouth counted " + std::to_string(head.getMouthTickCount())
                                                       + " ticks in 5");

        arena.mLevel->setBlockState(at.x + 1, at.y, at.z, BlockState("minecraft:air"));
        arena.update(at);
        require(!head.isDoingAnimation(), "the dragon head kept moving once unpowered");
        arena.mLevel->getBlockActors().tick(arena.mOwner);
        require(head.getMouthTickCount() == 5, "a still mouth kept counting");
    }

    // the piglin head moves too; other heads stay still whatever the power
    void otherHeads(Arena &arena) {
        const Vector3i piglin(28, 100, 0);
        SkullBlockActor &moving = arena.head("minecraft:piglin_head", piglin);
        arena.mLevel->setBlockState(piglin.x + 1, piglin.y, piglin.z, arena.block("minecraft:redstone_block"));
        arena.update(piglin);
        require(moving.isDoingAnimation(), "a powered piglin head does not move its mouth");

        const Vector3i zombie(32, 100, 0);
        SkullBlockActor &still = arena.head("minecraft:zombie_head", zombie);
        arena.mLevel->setBlockState(zombie.x + 1, zombie.y, zombie.z, arena.block("minecraft:redstone_block"));
        arena.update(zombie);
        require(!still.isDoingAnimation(), "a powered zombie head moves its mouth");
    }

    // a player wearing a zombie head is found by a zombie from half as far
    void disguise(Arena &arena) {
        MobActor &zombie = arena.mob("minecraft:zombie", Vector3f(40.5f, 100.0f, 0.5f));
        ServerPlayer &player = arena.player(9001, Vector3f(52.5f, 100.0f, 0.5f));
        NearestAttackableTargetGoal goal(16.0f, {}, 0, NearestAttackableTargetGoal::Sight());
        require(goal.canUse(arena.mOwner, zombie), "a zombie did not find a player 12 blocks off");

        player.getInventory().setArmor(PlayerInventory::ARMOR_HEAD, stack("minecraft:zombie_head"));
        require(!goal.canUse(arena.mOwner, zombie), "a zombie found a player wearing its head 12 blocks off");
        player.setPosition(Vector3f(47.5f, 100.0f, 0.5f));
        require(goal.canUse(arena.mOwner, zombie), "a zombie lost a player wearing its head 7 blocks off");

        // another kind's head hides nothing
        player.setPosition(Vector3f(52.5f, 100.0f, 0.5f));
        player.getInventory().setArmor(PlayerInventory::ARMOR_HEAD, stack("minecraft:creeper_head"));
        require(goal.canUse(arena.mOwner, zombie), "a creeper head hid a player from a zombie");

        // a skeleton is fooled by its own skull
        MobActor &skeleton = arena.mob("minecraft:skeleton", Vector3f(40.5f, 100.0f, 10.5f));
        player.setPosition(Vector3f(52.5f, 100.0f, 10.5f));
        player.getInventory().setArmor(PlayerInventory::ARMOR_HEAD, stack("minecraft:skeleton_skull"));
        require(!goal.canUse(arena.mOwner, skeleton), "a skeleton found a player wearing its skull 12 blocks off");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-heads-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        Arena arena(directory);
        dragonMouth(arena);
        otherHeads(arena);
        disguise(arena);
    } catch (const std::exception &error) {
        std::cout << "Head regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
