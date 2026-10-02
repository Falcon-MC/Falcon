#include "Command/CommandMap.h"

#include <algorithm>
#include <cctype>

namespace {
    std::string toLowerCase(const std::string &value) {
        std::string lowered = value;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                       [](unsigned char character) { return (char) std::tolower(character); });
        return lowered;
    }
}

void CommandMap::registerCommand(std::shared_ptr<Command> command) {
    if (command == nullptr)
        return;

    mByName[toLowerCase(command->getName())] = command.get();
    for (const std::string &alias: command->getAliases())
        mByName[toLowerCase(alias)] = command.get();

    mCommands.push_back(std::move(command));
}

void CommandMap::unregisterCommand(const std::string &name) {
    Command *command = getCommand(name);
    if (command == nullptr)
        return;

    for (auto it = mByName.begin(); it != mByName.end();) {
        if (it->second == command)
            it = mByName.erase(it);
        else
            ++it;
    }

    mCommands.erase(std::remove_if(mCommands.begin(), mCommands.end(),
                                   [command](const std::shared_ptr<Command> &entry) {
                                       return entry.get() == command;
                                   }), mCommands.end());
}

Command *CommandMap::getCommand(const std::string &name) const {
    auto it = mByName.find(toLowerCase(name));
    return it == mByName.end() ? nullptr : it->second;
}

std::vector<std::string> CommandMap::_tokenize(const std::string &commandLine) {
    std::vector<std::string> tokens;
    std::string token;
    bool quoted = false;
    bool escaped = false;
    bool started = false;

    for (char character: commandLine) {
        if (escaped) {
            token.push_back(character);
            escaped = false;
            continue;
        }

        if (quoted && character == '\\') {
            escaped = true;
            continue;
        }

        if (character == '"') {
            quoted = !quoted;
            started = true;
            continue;
        }

        if (!quoted && std::isspace((unsigned char) character)) {
            if (started) {
                tokens.push_back(token);
                token.clear();
                started = false;
            }
            continue;
        }

        token.push_back(character);
        started = true;
    }

    if (started)
        tokens.push_back(token);

    return tokens;
}

std::string CommandMap::_skipFields(const std::string &commandLine, size_t count) {
    size_t index = 0;
    size_t skipped = 0;

    while (index < commandLine.size() && skipped < count) {
        while (index < commandLine.size() && std::isspace((unsigned char) commandLine[index]) != 0)
            ++index;

        if (index >= commandLine.size())
            break;

        while (index < commandLine.size() && std::isspace((unsigned char) commandLine[index]) == 0)
            ++index;

        ++skipped;
    }

    while (index < commandLine.size() && std::isspace((unsigned char) commandLine[index]) != 0)
        ++index;

    return commandLine.substr(index);
}

bool CommandMap::dispatch(CommandOrigin &sender, const std::string &commandLine) {
    std::string line = commandLine;
    if (!line.empty() && line[0] == '/')
        line.erase(0, 1);

    std::vector<std::string> tokens = _tokenize(line);
    if (tokens.empty())
        return false;

    Command *command = getCommand(tokens[0]);
    if (command == nullptr) {
        sender.sendTranslation("commands.generic.unknown", {tokens[0]});
        return false;
    }

    if ((int) sender.getCommandPermission() < (int) command->getRequiredPermission()) {
        sender.sendTranslation("commands.generic.error.permissions", {tokens[0]});
        return false;
    }

    std::vector<std::string> arguments(tokens.begin() + 1, tokens.end());

    const size_t rawIndex = command->getRawArgumentIndex();
    if (rawIndex != (size_t) -1 && arguments.size() > rawIndex) {
        const std::string raw = _skipFields(line, rawIndex + 1);
        if (!raw.empty()) {
            arguments.resize(rawIndex);
            arguments.push_back(raw);
        }
    }

    return command->execute(sender, arguments);
}

std::vector<Command *> CommandMap::getCommands() const {
    std::vector<Command *> commands;
    commands.reserve(mCommands.size());

    for (const std::shared_ptr<Command> &command: mCommands)
        commands.push_back(command.get());

    return commands;
}
