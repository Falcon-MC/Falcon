#include "Actor/Definition/EntityDefinitions.h"

#include "Core/Debug/BedrockLog.h"
#include "EntityDefinitionsJson.h"

std::unique_ptr<json::Value> &EntityDefinitions::_root() {
    static std::unique_ptr<json::Value> root;
    return root;
}

void EntityDefinitions::initialize() {
    _root() = json::parse(FalconEntityData::kEntityDefinitionsJson);
    if (_root() == nullptr || !_root()->isObject()) {
        _root().reset();
        LOG_WARN(LogAreaID::Server, "Could not parse the embedded entity definitions");
    }
}

const json::Value *EntityDefinitions::find(const std::string &identifier) {
    return _root() == nullptr ? nullptr : _root()->get(identifier);
}
