#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/Systems/RandomTickSystem.h"

#include <algorithm>

namespace {
    class MelonBlock final : public Block {
    public:
        explicit MelonBlock(const Block &block) : Block(block) {}

        static bool matches(const std::string &identifier) {
            return identifier == "minecraft:melon_block";
        }

        bool getDrops(const BlockState &state, const ItemStack &tool, int32_t fortune,
                      std::vector<BlockDrop> &drops) const override {
            if (hasSilkTouch(tool)) {
                drops.push_back({state.mName, 1});
            } else {
                const int count = 3 + RandomTickSystem::nextInt(5) + RandomTickSystem::nextInt(fortune + 1);
                drops.push_back({"minecraft:melon_slice", std::min(9, count)});
            }
            return true;
        }
    };
}

FALCON_REGISTER_BLOCK(MelonBlock, 275);
