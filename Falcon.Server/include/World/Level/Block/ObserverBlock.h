#pragma once

#include "World/Level/Block/FacingMachineBlock.h"

#include <string>

class ObserverBlock final : public FacingMachineBlock {
public:
    explicit ObserverBlock(const Block &block) : FacingMachineBlock(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool isSignalSource() const override;
};
