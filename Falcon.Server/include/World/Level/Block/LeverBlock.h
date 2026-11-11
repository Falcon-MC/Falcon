#pragma once

#include "World/Level/Block/Block.h"
#include "Core/Math/Vector3i.h"

#include <string>

class ServerNetworkHandler;
class ServerPlayer;

class LeverBlock final : public Block {
public:
    explicit LeverBlock(const Block &block) : Block(block) {}

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    static bool matches(const std::string &identifier);

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;

    bool isSignalSource() const override;

    bool watchesNeighbour(const Vector3i &position, const BlockState &state, const Vector3i *changed) const override;

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;

    void onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state) const override;

private:
    static int _attachedFace(const BlockState &state);
};
