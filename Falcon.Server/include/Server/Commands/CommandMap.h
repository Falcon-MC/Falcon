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

    // in place of the command's aliases; one another command already answers to is skipped
    void setAliases(Command &command, const std::vector<std::string> &aliases);

    bool dispatch(CommandOrigin &sender, const std::string &commandLine);

    std::vector<Command *> getCommands() const;

    std::vector<std::string> complete(const std::string &line, const std::vector<std::string> &playerNames,
                                      size_t &replacementStart) const;

    // starts: where each token begins in the line, for a command that needs the text behind one untouched
    static std::vector<std::string> tokenize(const std::string &commandLine, std::vector<size_t> *starts = nullptr,
                                             bool *endsInToken = nullptr);

    // ~~5~ written without spaces becomes ~, ~5 and ~; starts, when given, follow the split tokens
    static void splitRelativeCoordinates(std::vector<std::string> &tokens, std::vector<size_t> *starts = nullptr);

private:

    static std::string _skipFields(const std::string &commandLine, size_t count);

    std::vector<std::shared_ptr<Command>> mCommands;
    std::unordered_map<std::string, Command *> mByName;
};
