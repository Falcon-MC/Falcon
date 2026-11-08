#include "World/Actor/AI/Goal/MobAttack.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/Hostile/EndermiteActor.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/MobEffect.h"

#include <iostream>
#include <memory>
#include <stdexcept>

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

        Tag saved = endermite.saveNbt();
        require(saved.getInt("Lifetime", -1) == EndermiteActor::MAX_LIFETIME, "lifetime not saved");
        named.loadNbt(saved);
        require(named.getLifetime() == EndermiteActor::MAX_LIFETIME, "lifetime not loaded");
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
    try {
        EntityDefinitions::initialize();
        attackEffects();
        endermiteLifetime();
        cobwebImmunity();
        std::cout << "hostile melee mob checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
