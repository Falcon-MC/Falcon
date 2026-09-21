#pragma once

#include "Command/Command.h"

class TestForBlockCommand : public Command {
public:
    TestForBlockCommand();

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;
};
