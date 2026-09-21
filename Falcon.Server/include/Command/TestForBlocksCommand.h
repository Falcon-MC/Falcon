#pragma once

#include "Command/Command.h"

class TestForBlocksCommand : public Command {
public:
    TestForBlocksCommand();

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;
};
