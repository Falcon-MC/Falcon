#include "Core/NBT/Tag.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/Hostile/WardenActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/SculkShriekerBlockActor.h"
#include "World/Level/Block/SculkShriekerBlock.h"
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
    const int32_t FLOOR_Y = 99;
    const Vector3i SHRIEKER(0, FLOOR_Y, 0);

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Sculk shrieker", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        explicit Arena(bool canSummon) {
            mLevel.setOwner(&mOwner);
            for (int32_t x = -3; x <= 3; x++) {
                for (int32_t z = -3; z <= 3; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            const BlockState stone("minecraft:stone");
            for (int32_t x = -40; x < 40; x++) {
                for (int32_t z = -40; z < 40; z++)
                    mLevel.setBlockState(x, FLOOR_Y, z, stone);
            }
            Tag states = Tag::ofCompound();
            states.putByte(SculkShriekerBlock::STATE_ACTIVE, 0);
            states.putByte(SculkShriekerBlock::STATE_CAN_SUMMON, canSummon ? 1 : 0);
            const BlockState shrieker("minecraft:sculk_shrieker", states);
            mLevel.setBlock(SHRIEKER, shrieker, true);
            VanillaBlocks::fromState(shrieker)->onPlacedInLevel(mOwner, mLevel, SHRIEKER, shrieker);
            mOwner.setDifficulty("normal");
        }

        ServerPlayer &player(uint64_t runtimeId, const Vector3f &position) {
            const NetworkIdentifier id("shrieker-player", runtimeId);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            player.moveToLevel(mLevel);
            player.setPosition(position);
            player.setGameType((int32_t) GameType::Survival);
            return player;
        }

        SculkShriekerBlockActor &shrieker() {
            auto *actor = mLevel.getBlockActors().find<SculkShriekerBlockActor>(SHRIEKER);
            require(actor != nullptr, "the shrieker has no block actor");
            return *actor;
        }

        bool active() {
            return mLevel.getBlockState(SHRIEKER.x, SHRIEKER.y, SHRIEKER.z)
                           .mStates.getByte(SculkShriekerBlock::STATE_ACTIVE, 0)
                   != 0;
        }

        void stepOn(ServerPlayer &player) {
            const BlockState state = mLevel.getBlockState(SHRIEKER.x, SHRIEKER.y, SHRIEKER.z);
            VanillaBlocks::fromState(state)->onStepOn(mOwner, player, SHRIEKER, state);
        }

        // the scheduled end of the shriek
        void endShriek() {
            const BlockState state = mLevel.getBlockState(SHRIEKER.x, SHRIEKER.y, SHRIEKER.z);
            VanillaBlocks::fromState(state)->onScheduledUpdate(mOwner, mLevel, SHRIEKER, state);
        }

        int32_t wardens() {
            int32_t count = 0;
            mOwner.forEachActorNear(Vector3f(0.5f, FLOOR_Y + 1.0f, 0.5f), 32.0f, [&count](ServerActor &actor) {
                if (dynamic_cast<WardenActor *>(&actor) != nullptr)
                    ++count;
            });
            return count;
        }
    };

    Vector3f at(float x, float z) {
        return Vector3f(x, FLOOR_Y + 1.0f, z);
    }

    // down a level every ten minutes, two hundred ticks of cooldown, saved under the game's names
    void tracker() {
        WardenSpawnTracker tracker;
        tracker.increase();
        require(tracker.mLevel == 1 && tracker.mCooldown == 200 && tracker.mDecreaseTimer == 12000,
                "a warning did not set level, cooldown and timer");
        for (int32_t tick = 0; tick < 200; tick++)
            tracker.tick();
        require(!tracker.onCooldown(), "the cooldown outlasted 200 ticks");
        for (int32_t tick = 0; tick < 11800; tick++)
            tracker.tick();
        require(tracker.mLevel == 0, "the level did not fall after 12000 ticks");
        tracker.mLevel = 4;
        tracker.increase();
        require(tracker.mLevel == 4, "the level went past 4");

        Tag saved = Tag::ofCompound();
        tracker.save(saved);
        WardenSpawnTracker loaded;
        loaded.load(saved);
        require(saved.getInt("WardenThreatLevel", -1) == 4 && loaded.mLevel == 4 && loaded.mCooldown == 200,
                "the tracker did not save under WardenThreatLevel");
    }

    // a step warns once, every player near shares it, the reply ends the shriek with darkness
    void warning() {
        Arena arena(true);
        ServerPlayer &walker = arena.player(9300, at(0.5f, 0.5f));
        ServerPlayer &near = arena.player(9301, at(10.5f, 0.5f));
        ServerPlayer &far = arena.player(9302, at(30.5f, 0.5f));
        near.getWardenSpawnTracker().mLevel = 2;

        arena.stepOn(walker);
        require(arena.active(), "a step did not make it shriek");
        require(arena.shrieker().getWarningLevel() == 3, "the most warned player's level was not raised");
        require(walker.getWardenSpawnTracker().mLevel == 3 && near.getWardenSpawnTracker().mLevel == 3,
                "the players near did not share the warning");
        require(far.getWardenSpawnTracker().mLevel == 0, "a player past 16 blocks was warned");

        arena.stepOn(walker);
        require(walker.getWardenSpawnTracker().mLevel == 3, "an active shrieker warned again");
        arena.endShriek();
        require(!arena.active(), "the shriek did not end");
        require(walker.hasEffect(MobEffectId::Darkness) && far.hasEffect(MobEffectId::Darkness),
                "the reply did not bring darkness within 40 blocks");
        require(arena.wardens() == 0, "a warden came up before the fourth warning");

        // still on cooldown: no shriek at all
        arena.stepOn(walker);
        require(!arena.active(), "it shrieked for a player on cooldown");
    }

    // the fourth warning calls a warden up out of the ground
    void summon() {
        Arena arena(true);
        ServerPlayer &walker = arena.player(9310, at(0.5f, 0.5f));
        walker.getWardenSpawnTracker().mLevel = 3;
        arena.stepOn(walker);
        require(arena.shrieker().getWarningLevel() == 4, "the fourth warning was not reached");
        arena.endShriek();
        require(arena.wardens() == 1, "no warden came up at the fourth warning");
        int32_t emerging = 0;
        arena.mOwner.forEachActorNear(at(0.5f, 0.5f), 32.0f, [&emerging](ServerActor &actor) {
            if (auto *warden = dynamic_cast<WardenActor *>(&actor))
                emerging += warden->getSpawnEvent() == "minecraft:spawn_emerging" ? 1 : 0;
        });
        require(emerging == 1, "the warden did not come up emerging");

        // with a warden around, no shriek
        walker.getWardenSpawnTracker().mCooldown = 0;
        arena.stepOn(walker);
        require(!arena.active(), "it shrieked with a warden near");
    }

    // a placed shrieker shrieks but never warns
    void placed() {
        Arena arena(false);
        ServerPlayer &walker = arena.player(9320, at(0.5f, 0.5f));
        arena.stepOn(walker);
        require(arena.active(), "a placed shrieker did not shriek");
        require(!arena.shrieker().getWarningLevel().has_value() && walker.getWardenSpawnTracker().mLevel == 0,
                "a shrieker that cannot summon warned");
        arena.endShriek();
        require(!walker.hasEffect(MobEffectId::Darkness), "a shrieker that cannot summon brought darkness");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-sculk-shrieker-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        tracker();
        warning();
        summon();
        placed();
        std::cout << "sculk shrieker checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
