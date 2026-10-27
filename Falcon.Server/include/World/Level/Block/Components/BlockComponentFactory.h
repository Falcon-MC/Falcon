#pragma once

#include "World/Level/Block/Components/BlockBehavior.h"

#include <string>

class BlockComponentFactory {
public:
    static const BlockBehavior &getBehavior(const std::string &identifier);
};
