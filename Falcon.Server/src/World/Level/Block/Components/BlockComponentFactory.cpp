#include "World/Level/Block/Components/BlockComponentFactory.h"

#include "World/Level/Block/Components/BlockBehaviorRegistry.h"

const BlockBehavior &BlockComponentFactory::getBehavior(const std::string &identifier) {
    return BlockBehaviorRegistry::get(identifier);
}
