#pragma once

#include "Core/Json/Json.h"

#include <memory>
#include <string>

class Actor;
class MobActor;
class ServerActor;
class ServerNetworkHandler;
class ActorDamageSource;

class EntityFilter {
public:
    // exposes the evaluated damage to has_damage; filters run on the main thread only: one scope at a time
    class DamageScope {
    public:
        DamageScope(const ActorDamageSource &source, float amount);

        ~DamageScope();

        DamageScope(const DamageScope &) = delete;

        DamageScope &operator=(const DamageScope &) = delete;
    };

    struct Test;

    // a filter compiled into typed tests; it keeps nothing of the json it comes from
    struct Group;

    // nullptr for no filter
    static std::shared_ptr<const Group> compile(const json::Value *filter);

    // every filter of the definitions, compiled once: root must outlive them, up to the next call; nullptr drops them
    static void compileDefinitions(const json::Value *root);

    static void clearPackDefinitions();

    static void compilePackDefinition(const json::Value &definition);

    static bool test(const Group &filter, ServerNetworkHandler &owner, const ServerActor &self,
                     const Actor *other = nullptr);

    // a filter outside the compiled definitions is compiled again on every call
    static bool test(const json::Value &filter, ServerNetworkHandler &owner, const ServerActor &self,
                     const Actor *other = nullptr);

    // a player is only of the player family
    static bool hasFamily(const Actor &actor, const std::string &family);
};

// a filter compiled when it is set from json: a goal keeps it and tests it without reading the json again
class CompiledEntityFilter {
public:
    CompiledEntityFilter() = default;

    CompiledEntityFilter(std::nullptr_t) {
    }

    CompiledEntityFilter(const std::shared_ptr<json::Value> &filter) : mGroup(EntityFilter::compile(filter.get())) {
    }

    CompiledEntityFilter(const std::unique_ptr<json::Value> &filter) : mGroup(EntityFilter::compile(filter.get())) {
    }

    bool operator==(std::nullptr_t) const {
        return mGroup == nullptr;
    }

    const EntityFilter::Group &operator*() const {
        return *mGroup;
    }

private:
    std::shared_ptr<const EntityFilter::Group> mGroup;
};
