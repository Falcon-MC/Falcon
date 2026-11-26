#pragma once

#include "World/Level/Block/BaseRailBlock.h"

#include <string>

// powered and activator rails: the circuit powers them from a direct source, or along up to eight rails of their kind
class PoweredRailBlock final : public BaseRailBlock {
public:
    static constexpr const char *STATE_POWERED = "rail_data_bit";

    explicit PoweredRailBlock(const Block &base) : BaseRailBlock(base) {}

    static bool matches(const std::string &identifier);
};
