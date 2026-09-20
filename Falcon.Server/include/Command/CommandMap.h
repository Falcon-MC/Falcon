#pragma once

#include "Command/Command.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class CommandMap {
public:
    void registerCommand(std::shared_ptr<Command> command);

    Command *getCommand(const std::string &name) const;

    bool dispatch(CommandOrigin &sender, const std::string &commandLine);

    std::vector<Command *> getCommands() const;

private:
    static std::vector<std::string> _tokenize(const std::string &commandLine);

    static std::string _skipFields(const std::string &commandLine, size_t count);

    std::vector<std::shared_ptr<Command>> mCommands;
    std::unordered_map<std::string, Command *> mByName;
};
