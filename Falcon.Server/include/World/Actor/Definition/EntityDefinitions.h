#pragma once

#include "World/Actor/ActorPropertySchema.h"
#include "Core/Json/Json.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class EntityDefinitions {
public:
    static void initialize();

    static const json::Value *find(const std::string &identifier);

    static constexpr uint32_t NO_COMPONENT = UINT32_MAX;

    // a number per component name of the definitions, given when they load and kept for the process; NO_COMPONENT
    // for a name no definition has
    static uint32_t findComponentId(std::string_view name);

    static const std::string &getComponentName(uint32_t id);

    static const std::vector<ActorPropertyDescription> *findProperties(const std::string &identifier);

    static const std::map<std::string, std::vector<ActorPropertyDescription>> &getAllProperties();

private:
    static std::unique_ptr<json::Value> &_root();

    static std::map<std::string, std::vector<ActorPropertyDescription>> &_properties();
};
