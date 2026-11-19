#include "World/Level/Block/WitherProofBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockIdentifier.h"

FALCON_REGISTER_BLOCK(WitherProofBlock, 206);

bool WitherProofBlock::matches(const std::string &identifier) {
    return BlockIdentifier::equalsAny(identifier,
                                      {"minecraft:bedrock", "minecraft:invisible_bedrock", "minecraft:barrier",
                                       "minecraft:border_block", "minecraft:allow", "minecraft:deny",
                                       "minecraft:chalkboard", "minecraft:camera", "minecraft:structure_block",
                                       "minecraft:structure_void", "minecraft:reinforced_deepslate"});
}
