#pragma once

#include "World/Level/Block/Block.h"
#include "World/Level/Block/BlockTypeIds.h"

#include "Core/Math/Vector3i.h"

#include <string>

class Level;
class ServerNetworkHandler;
class ServerPlayer;

class AnvilBlock final : public Block {
public:
    AnvilBlock();

    explicit AnvilBlock(const Block &block);

    static bool matches(const BlockState &state);

    static bool matches(const std::string &identifier);

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    // outside creative, 12% chance to drop one damage stage; past the last stage it breaks
    static void onUsed(ServerNetworkHandler &owner, Level &level, const Vector3i &position, bool creative);
};
