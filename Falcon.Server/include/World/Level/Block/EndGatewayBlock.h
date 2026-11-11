#pragma once

#include "World/Level/Block/Block.h"

#include <string>

/** The end gateway: walked into like a portal, its block actor does the teleporting (EndGatewayBlockActor). */
class EndGatewayBlock final : public Block {
public:
    explicit EndGatewayBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    bool getCollisionShape(const BlockState &state, const Actor *actor, AxisAlignedBB &shape) const override;

    void onActorInside(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                       const BlockState &state) const override;
};
