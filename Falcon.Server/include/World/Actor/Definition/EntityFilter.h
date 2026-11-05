#pragma once

#include "Core/Json/Json.h"

#include <string>

class Actor;
class MobActor;
class ServerNetworkHandler;
class ActorDamageSource;

class EntityFilter {
public:
    /**
     * Exposes the damage being evaluated to `has_damage` while a damage sensor runs its filters.
     * Filters are only ever tested on the main thread, so one active scope at a time is enough.
     */
    class DamageScope {
    public:
        DamageScope(const ActorDamageSource &source, float amount);

        ~DamageScope();

        DamageScope(const DamageScope &) = delete;

        DamageScope &operator=(const DamageScope &) = delete;
    };

    static bool test(const json::Value &filter, ServerNetworkHandler &owner, const MobActor &self,
                     const Actor *other = nullptr);

    /** Whether the actor's `minecraft:type_family` lists family; a player is only of the `player` family. */
    static bool hasFamily(const Actor &actor, const std::string &family);

private:
    static bool _testAll(const json::Value &filters, ServerNetworkHandler &owner, const MobActor &self,
                         const Actor *other);

    static bool _testAny(const json::Value &filters, ServerNetworkHandler &owner, const MobActor &self,
                         const Actor *other);

    static bool _testSingle(const json::Value &filter, ServerNetworkHandler &owner, const MobActor &self,
                            const Actor *other);

    static bool _testBlock(const std::string &test, const std::string &op, const json::Value *value,
                           ServerNetworkHandler &owner, const MobActor &self);
};
