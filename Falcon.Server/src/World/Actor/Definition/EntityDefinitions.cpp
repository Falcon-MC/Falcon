#include "World/Actor/Definition/EntityDefinitions.h"

#include "Core/Debug/BedrockLog.h"
#include "Scripting/BehaviorPackManager.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "EntityDefinitionsJson.h"

#include <deque>
#include <unordered_map>

namespace {
    struct ComponentIds {
        // a deque keeps every name in place: the keys of the index point into it
        std::deque<std::string> mNames;
        std::unordered_map<std::string_view, uint32_t> mIds;
    };

    ComponentIds &componentIds() {
        static ComponentIds ids;
        return ids;
    }

    std::map<std::string, std::unique_ptr<json::Value>, std::less<>> packDefinitions;

    void internComponents(const json::Value *components) {
        if (components == nullptr)
            return;

        ComponentIds &ids = componentIds();
        for (const std::string &name: components->mKeys) {
            if (ids.mIds.find(name) != ids.mIds.end())
                continue;
            ids.mNames.push_back(name);
            ids.mIds.emplace(ids.mNames.back(), (uint32_t) (ids.mNames.size() - 1));
        }
    }
}

std::unique_ptr<json::Value> &EntityDefinitions::_root() {
    static std::unique_ptr<json::Value> root;
    return root;
}

std::map<std::string, std::vector<ActorPropertyDescription>> &EntityDefinitions::_properties() {
    static std::map<std::string, std::vector<ActorPropertyDescription>> properties;
    return properties;
}

void EntityDefinitions::initialize() {
    // the compiled filters point into the definitions they come from
    EntityFilter::compileDefinitions(nullptr);
    _root() = json::parse(FalconEntityData::kEntityDefinitionsJson);
    _properties().clear();
    if (_root() == nullptr || !_root()->isObject()) {
        _root().reset();
        LOG_WARN(LogAreaID::Server, "Could not parse the embedded entity definitions");
        return;
    }

    for (const std::string &identifier: _root()->mKeys) {
        const json::Value *definition = _root()->get(identifier);
        internComponents(definition->get("components"));
        if (const json::Value *groups = definition->get("component_groups")) {
            for (const std::string &group: groups->mKeys)
                internComponents(groups->get(group));
        }

        const json::Value *description = definition->get("description");
        const json::Value *properties = description == nullptr ? nullptr : description->get("properties");
        if (properties == nullptr)
            continue;

        std::vector<ActorPropertyDescription> schema = ActorPropertySchema::parse(*properties);
        if (!schema.empty())
            _properties()[identifier] = std::move(schema);
    }
    EntityFilter::compileDefinitions(_root().get());
}

void EntityDefinitions::loadPacks(const std::vector<BehaviorPack> &packs) {
    packDefinitions.clear();
    for (const auto &pack: packs) {
        pack.visitJson("entities", [](const json::Value &doc) {
            const auto *definition = doc.get("minecraft:entity");
            const auto *description = definition == nullptr ? nullptr : definition->get("description");
            const auto *identifier = description == nullptr ? nullptr : description->get("identifier");
            if (identifier == nullptr || !identifier->isString() || identifier->mString.empty())
                return;

            internComponents(definition->get("components"));
            if (const auto *groups = definition->get("component_groups")) {
                for (const auto &group: groups->mKeys)
                    internComponents(groups->get(group));
            }
            packDefinitions[identifier->mString] = definition->clone();
        });
    }
}

const json::Value *EntityDefinitions::find(const std::string &identifier) {
    if (const auto pack = packDefinitions.find(identifier); pack != packDefinitions.end())
        return pack->second.get();
    return _root() == nullptr ? nullptr : _root()->get(identifier);
}

uint32_t EntityDefinitions::findComponentId(std::string_view name) {
    const ComponentIds &ids = componentIds();
    const auto found = ids.mIds.find(name);
    return found == ids.mIds.end() ? NO_COMPONENT : found->second;
}

const std::string &EntityDefinitions::getComponentName(uint32_t id) {
    return componentIds().mNames[id];
}

const std::vector<ActorPropertyDescription> *EntityDefinitions::findProperties(const std::string &identifier) {
    const auto found = _properties().find(identifier);
    return found == _properties().end() ? nullptr : &found->second;
}

const std::map<std::string, std::vector<ActorPropertyDescription>> &EntityDefinitions::getAllProperties() {
    return _properties();
}
