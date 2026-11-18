#pragma once

#include "World/Level/Block/BaseRailBlock.h"

#include <string>

// powered and activator rails: powered from the redstone around them, or along up to eight rails of their kind
class PoweredRailBlock final : public BaseRailBlock {
public:
    static constexpr const char *STATE_POWERED = "rail_data_bit";
    static constexpr int MAX_DISTANCE = 8;

    explicit PoweredRailBlock(const Block &base) : BaseRailBlock(base) {}

    static bool matches(const std::string &identifier);

    void onRedstonePower(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                         const BlockState &state) const override;

    void onPlacedInLevel(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                         const BlockState &state) const override;

private:
    static bool _findSignal(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state, bool forward, int distance);

    static bool _isPoweredRail(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                               const std::string &name, bool forward, int distance, int direction);
};
