#pragma once

#include "Actor/ActorPropertySchema.h"
#include "Core/Json/Json.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

class EntityDefinitions {
public:
    static void initialize();

    static const json::Value *find(const std::string &identifier);

    static const std::vector<ActorPropertyDescription> *findProperties(const std::string &identifier);

    static const std::map<std::string, std::vector<ActorPropertyDescription>> &getAllProperties();

private:
    static std::unique_ptr<json::Value> &_root();

    static std::map<std::string, std::vector<ActorPropertyDescription>> &_properties();
};
