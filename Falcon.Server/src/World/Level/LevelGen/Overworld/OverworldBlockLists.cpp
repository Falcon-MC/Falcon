#include "World/Level/LevelGen/Overworld/OverworldBlockLists.h"

#include "World/Level/Block/VanillaBlocks.h"

const BlockStateSet &OverworldBlockLists::sand() {
    static const BlockStateSet SAND = {
            VanillaBlocks::SAND().getIdentifier(), VanillaBlocks::RED_SAND().getIdentifier(),
            VanillaBlocks::SUSPICIOUS_SAND().getIdentifier()
    };
    return SAND;
}
