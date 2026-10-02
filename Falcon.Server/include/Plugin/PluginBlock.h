#pragma once

#include "Block/Block.h"

#include <string>

class PluginBlock : public Block {
public:
    explicit PluginBlock(const Block &base);

    static bool matches(const std::string &identifier);

    static bool interactAt(ServerPlayer &player, const Vector3i &position, uint32_t face,
                           const std::string &identifier);

    static void brokenBy(ServerPlayer &player, const Vector3i &position, const std::string &identifier);

    bool getDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                  std::vector<BlockDrop> &drops) const override;
};
