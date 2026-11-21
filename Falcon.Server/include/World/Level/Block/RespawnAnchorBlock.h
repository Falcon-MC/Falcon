#pragma once

#include "World/Level/Block/Block.h"

#include <optional>
#include <string>

class RespawnAnchorBlock final : public Block {
public:
    static constexpr int MAX_CHARGE = 4;

    explicit RespawnAnchorBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    static int getCharge(const BlockState &state);

    static void addCharge(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int delta);

    static bool isChargedAnchor(Level &level, const Vector3i &position);

    static bool isSurroundedByWater(Level &level, const Vector3i &position);

    static std::optional<Vector3i> findStandUpPosition(Level &level, const Vector3i &position);

    // empty when the anchor no longer holds the spawn point; the charge is spent by the caller
    static std::optional<Vector3f> findRespawnPosition(Level &level, const Vector3i &anchor,
                                                       const Vector3i &standPosition);

    static void explode(ServerNetworkHandler &owner, Level &level, const Vector3i &position);

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    int getLightEmission(const BlockState &state, int dataEmission) const override;

    int getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const override;
};
