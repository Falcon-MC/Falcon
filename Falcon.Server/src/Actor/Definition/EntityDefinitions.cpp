#include "Actor/Definition/EntityDefinitions.h"

#include "Core/Debug/BedrockLog.h"
#include "EntityDefinitionsJson.h"

std::unique_ptr<json::Value> &EntityDefinitions::_root() {
    static std::unique_ptr<json::Value> root;
    return root;
}

std::map<std::string, std::vector<ActorPropertyDescription>> &EntityDefinitions::_properties() {
    static std::map<std::string, std::vector<ActorPropertyDescription>> properties;
    return properties;
}

void EntityDefinitions::initialize() {
    _root() = json::parse(FalconEntityData::kEntityDefinitionsJson);
    _properties().clear();
    if (_root() == nullptr || !_root()->isObject()) {
        _root().reset();
        LOG_WARN(LogAreaID::Server, "Could not parse the embedded entity definitions");
        return;
    }

    for (const std::string &identifier: _root()->mKeys) {
        const json::Value *description = _root()->get(identifier)->get("description");
        const json::Value *properties = description == nullptr ? nullptr : description->get("properties");
        if (properties == nullptr)
            continue;

        std::vector<ActorPropertyDescription> schema = ActorPropertySchema::parse(*properties);
        if (!schema.empty())
            _properties()[identifier] = std::move(schema);
    }
}

const json::Value *EntityDefinitions::find(const std::string &identifier) {
    return _root() == nullptr ? nullptr : _root()->get(identifier);
}

const std::vector<ActorPropertyDescription> *EntityDefinitions::findProperties(const std::string &identifier) {
    const auto found = _properties().find(identifier);
    return found == _properties().end() ? nullptr : &found->second;
}

const std::map<std::string, std::vector<ActorPropertyDescription>> &EntityDefinitions::getAllProperties() {
    return _properties();
}
