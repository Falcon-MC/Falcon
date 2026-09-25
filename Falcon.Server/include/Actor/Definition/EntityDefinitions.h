#pragma once

#include "Core/Json/Json.h"

#include <memory>
#include <string>

class EntityDefinitions {
public:
    static void initialize();

    static const json::Value *find(const std::string &identifier);

private:
    static std::unique_ptr<json::Value> &_root();
};
