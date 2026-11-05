#include "World/Level/LevelGen/BlockStateSet.h"

#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Block/VanillaBlocks.h"

#include <memory>
#include <vector>

BlockStateSet::BlockStateSet(std::initializer_list<std::string> identifiers) {
    for (const std::string &identifier: identifiers)
        _add(identifier);
}

BlockStateSet BlockStateSet::ofBlocks(const std::function<bool(const Block &)> &matches) {
    BlockStateSet set;
    for (const std::unique_ptr<Block> &block: VanillaBlocks::getAll()) {
        if (matches(*block))
            set._add(block->getIdentifier());
    }
    return set;
}

void BlockStateSet::_add(const std::string &identifier) {
    const Block *block = VanillaBlocks::fromIdentifier(identifier);
    if (block != nullptr)
        mHashes.insert(block->toBlockState().getHash());

    const std::vector<Tag> *permutations = BlockPaletteRegistry::getInstance().getPermutations(identifier);
    if (permutations == nullptr)
        return;

    for (const Tag &states: *permutations)
        mHashes.insert(BlockStateHasher::hash(identifier, states));
}
