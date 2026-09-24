#include "Block/Blocks/ObserverBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(ObserverBlock, 155);

namespace {
    const char *OBSERVER = "minecraft:observer";
}

bool ObserverBlock::matches(const std::string &identifier)
{
    return identifier == OBSERVER;
}

bool ObserverBlock::isSignalSource() const
{
    return true;
}
