#pragma once

#include "Server/Commands/Command.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class CommandMap {
public:
    void registerCommand(std::shared_ptr<Command> command);

    void unregisterCommand(const std::string &name);

    Command *getCommand(const std::string &name) const;

    bool dispatch(CommandOrigin &sender, const std::string &commandLine);

    std::vector<Command *> getCommands() const;

    std::vector<std::string> complete(const std::string &line, const std::vector<std::string> &playerNames,
                                      size_t &replacementStart) const;

    // starts: where each token begins in the line, for a command that needs the text behind one untouched
    static std::vector<std::string> tokenize(const std::string &commandLine, std::vector<size_t> *starts = nullptr,
                                             bool *endsInToken = nullptr);

private:

    static std::string _skipFields(const std::string &commandLine, size_t count);

    std::vector<std::shared_ptr<Command>> mCommands;
    std::unordered_map<std::string, Command *> mByName;
};
