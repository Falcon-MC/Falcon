#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/AI/Goal/MobAttack.h"
#include "World/Actor/AI/Goal/NearestAttackableTargetGoal.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/Hostile/EndermiteActor.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/MobEffect.h"
#include "World/Actor/Player/ServerPlayer.h"
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
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    std::unique_ptr<ServerActor> create(const char *identifier) {
        std::unique_ptr<ServerActor> actor = ActorClassRegistry::create(100, identifier);
        require(actor != nullptr && dynamic_cast<MobActor *>(actor.get()) != nullptr, "mob class missing");
        return actor;
    }

    void attackEffects() {
        auto husk = create("minecraft:husk");
        MobEffectInstance effect;
        require(MobAttack::attackEffect(static_cast<MobActor &>(*husk), effect), "husk gave no hunger");
        require(effect.mId == MobEffectId::Hunger && effect.mDuration == 600 && effect.mAmplifier == 0,
                "husk hunger is not 30 s at level I");

        auto zombie = create("minecraft:zombie");
        require(!MobAttack::attackEffect(static_cast<MobActor &>(*zombie), effect), "zombie gave an effect");

        auto caveSpider = create("minecraft:cave_spider");
        MobActor &spider = static_cast<MobActor &>(*caveSpider);
        spider.addComponentGroup("minecraft:spider_poison_easy");
        require(!MobAttack::attackEffect(spider, effect), "easy cave spider poisoned");
        spider.removeComponentGroup("minecraft:spider_poison_easy");
        spider.addComponentGroup("minecraft:spider_poison_normal");
        require(MobAttack::attackEffect(spider, effect) && effect.mId == MobEffectId::Poison
                && effect.mDuration == 140, "normal cave spider poison is not 7 s");
        spider.removeComponentGroup("minecraft:spider_poison_normal");
        spider.addComponentGroup("minecraft:spider_poison_hard");
        require(MobAttack::attackEffect(spider, effect) && effect.mDuration == 300,
                "hard cave spider poison is not 15 s");
    }

    void endermiteLifetime() {
        auto actor = create(EndermiteActor::IDENTIFIER);
        EndermiteActor &endermite = static_cast<EndermiteActor &>(*actor);
        for (int32_t tick = 1; tick < EndermiteActor::MAX_LIFETIME; ++tick)
            require(!endermite.advanceLifetime(), "endermite left before its lifetime");
        require(endermite.advanceLifetime(), "endermite outlived 2400 ticks");

        auto namedActor = create(EndermiteActor::IDENTIFIER);
        EndermiteActor &named = static_cast<EndermiteActor &>(*namedActor);
        named.setNameTag("Mite");
        for (int32_t tick = 0; tick < EndermiteActor::MAX_LIFETIME * 2; ++tick)
            require(!named.advanceLifetime(), "named endermite despawned");
        require(named.getLifetime() == 0, "named endermite aged");

        Tag limit = Tag::ofCompound();
        limit.putInt("Lifetime", EndermiteActor::MAX_LIFETIME);
        named.loadNbt(limit);
        require(named.advanceLifetime(), "named endermite past its lifetime stayed");

        Tag saved = endermite.saveNbt();
        require(saved.getInt("Lifetime", -1) == EndermiteActor::MAX_LIFETIME, "lifetime not saved");
        named.loadNbt(saved);
        require(named.getLifetime() == EndermiteActor::MAX_LIFETIME, "lifetime not loaded");
    }

    struct Arena {
        std::filesystem::path mDirectory;
        ServerNetworkHandler mOwner{"Hostile melee", "", 1};
        Level *mLevel = nullptr;

        Arena(const std::filesystem::path &directory, const char *difficulty) : mDirectory(directory) {
            const std::filesystem::path path = directory / (std::string(difficulty) + ".properties");
            std::ofstream(path) << "difficulty=" << difficulty << "\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            for (int32_t x = -1; x <= 1; x++) {
                for (int32_t z = -1; z <= 1; z++)
                    mLevel->insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
        }

        MobActor &spawn(const char *identifier, const Vector3f &position) {
            ServerActor *actor = mOwner.spawnActor(*mLevel, identifier, position);
            MobActor *mob = dynamic_cast<MobActor *>(actor);
            require(mob != nullptr, "mob did not spawn");
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

        size_t count(const char *identifier) {
            size_t found = 0;
            for (auto &entry: mOwner.getActors()) {
                if (entry.second->getTypeId() == identifier && !entry.second->isDead())
                    ++found;
            }
            return found;
        }
    };

    void deadTargets(const std::filesystem::path &directory) {
        Arena arena(directory, "hard");
        MobActor &husk = arena.spawn("minecraft:husk", Vector3f(0.5f, 100.0f, 0.5f));
        ServerPlayer &alive = arena.player(9001, Vector3f(1.5f, 100.0f, 0.5f));
        MobAttack::applyAttackEffect(arena.mOwner, husk, alive);
        require(alive.hasEffect(MobEffectId::Hunger), "a living target got no hunger");

        ServerPlayer &dead = arena.player(9002, Vector3f(1.5f, 100.0f, 1.5f));
        dead.setHealth(0.0f);
        dead.setDead(true);
        MobAttack::applyAttackEffect(arena.mOwner, husk, dead);
        require(!dead.hasEffect(MobEffectId::Hunger), "a killed target kept the hunger through its respawn");
    }

    void caveSpiderDifficulty(const std::filesystem::path &directory) {
        const struct {
            const char *mDifficulty;
            int32_t mTicks;
        } cases[] = {{"easy", 0}, {"normal", 140}, {"hard", 300}};
        for (const auto &entry: cases) {
            Arena arena(directory, entry.mDifficulty);
            MobActor &spider = arena.spawn("minecraft:cave_spider", Vector3f(0.5f, 100.0f, 0.5f));
            spider.fireEvent(arena.mOwner, "minecraft:become_hostile");
            MobEffectInstance effect;
            const bool poisons = MobAttack::attackEffect(spider, effect);
            require(poisons == (entry.mTicks > 0) && (!poisons || effect.mDuration == entry.mTicks),
                    "the difficulty picked the wrong cave spider poison group");
        }
    }

    void villagerConversion(const std::filesystem::path &directory) {
        Arena arena(directory, "hard");
        MobActor &zombie = arena.spawn("minecraft:zombie", Vector3f(0.5f, 100.0f, 0.5f));
        MobActor &villager = arena.spawn("minecraft:villager_v2", Vector3f(1.5f, 100.0f, 0.5f));
        arena.mOwner.damageActor(villager, 1000.0f, &zombie);
        require(villager.isDead(), "the fatal hit did not kill the villager");
        villager.tickDeath(arena.mOwner);
        require(villager.isExpired(), "the killed villager was not replaced");
        require(arena.count("minecraft:zombie_villager_v2") == 1, "no zombie villager took its place");

        // a transformation from another source (the lightning's become_witch) dies with the villager
        MobActor &struck = arena.spawn("minecraft:villager_v2", Vector3f(-1.5f, 100.0f, 0.5f));
        struck.addComponentGroup("become_witch");
        arena.mOwner.hurtActor(struck, 1000.0f, "death.attack.generic");
        require(struck.isDead(), "the generic hit did not kill the villager");
        struck.tickDeath(arena.mOwner);
        require(!struck.isExpired() && arena.count("minecraft:witch") == 0, "a dying villager became a witch");
    }

    void targetSight(const std::filesystem::path &directory) {
        Arena arena(directory, "hard");
        Level &level = *arena.mLevel;
        for (int32_t x = -1; x <= 2; x++) {
            for (int32_t y = 100; y <= 103; y++)
                level.setBlockState(x, y, 2, BlockState("minecraft:stone"));
        }

        MobActor &zombie = arena.spawn("minecraft:zombie", Vector3f(0.5f, 100.0f, 0.5f));
        ServerPlayer &hidden = arena.player(9101, Vector3f(0.5f, 100.0f, 4.5f));
        require(!zombie.canSee(arena.mOwner, hidden), "the wall did not block the sight");

        NearestAttackableTargetGoal::Sight sight;
        sight.mMustSee = true;
        sight.mForgetTicks = 5;
        NearestAttackableTargetGoal seeing(16.0f, {}, 0, sight);
        require(!seeing.canUse(arena.mOwner, zombie), "must_see acquired a hidden player");

        NearestAttackableTargetGoal blind(16.0f, {}, 0, NearestAttackableTargetGoal::Sight());
        require(blind.canUse(arena.mOwner, zombie), "without must_see the hidden player was ignored");

        ServerPlayer &visible = arena.player(9102, Vector3f(6.5f, 100.0f, 0.5f));
        require(seeing.canUse(arena.mOwner, zombie), "must_see ignored the visible player");
        seeing.start(arena.mOwner, zombie);
        require(zombie.getTarget(arena.mOwner) == &visible, "must_see picked the hidden nearer player");

        // forgotten once unseen for longer than must_see_forget_duration
        require(zombie.setTarget(arena.mOwner, hidden.getRuntimeId()), "could not aim at the hidden player");
        seeing.start(arena.mOwner, zombie);
        for (int32_t tick = 0; tick < sight.mForgetTicks; ++tick)
            require(seeing.canContinueToUse(arena.mOwner, zombie), "the target was forgotten too early");
        require(!seeing.canContinueToUse(arena.mOwner, zombie), "the unseen target was never forgotten");

        // reselect_targets switches to a nearer match at the next scan
        NearestAttackableTargetGoal::Sight reselect;
        reselect.mReselect = true;
        NearestAttackableTargetGoal switching(16.0f, {}, 1, reselect);
        require(zombie.setTarget(arena.mOwner, visible.getRuntimeId()), "could not aim at the far player");
        switching.start(arena.mOwner, zombie);
        require(switching.canContinueToUse(arena.mOwner, zombie), "reselect dropped its target");
        require(zombie.getTarget(arena.mOwner) == &hidden, "reselect_targets kept the farther target");
    }

    void cobwebImmunity() {
        for (const char *name: {"minecraft:spider", "minecraft:cave_spider"}) {
            auto spider = create(name);
            require(spider->ignoresMovementSlowdown("minecraft:web"), "spider slowed by cobweb");
            require(!spider->ignoresMovementSlowdown("minecraft:sweet_berry_bush"), "spider ignores berries");
        }
        auto zombie = create("minecraft:zombie");
        require(!zombie->ignoresMovementSlowdown("minecraft:web"), "zombie ignores cobweb");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory = std::filesystem::temp_directory_path()
            / ("falcon-melee-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        attackEffects();
        endermiteLifetime();
        cobwebImmunity();
        deadTargets(directory);
        caveSpiderDifficulty(directory);
        villagerConversion(directory);
        targetSight(directory);
        std::cout << "hostile melee mob checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
