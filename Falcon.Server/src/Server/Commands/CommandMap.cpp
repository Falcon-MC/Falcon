#include "Server/Commands/CommandMap.h"

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
                                   [command](const std::shared_ptr<Command> &entry) { return entry.get() == command; }),
                    mCommands.end());
}

Command *CommandMap::getCommand(const std::string &name) const {
    auto it = mByName.find(toLowerCase(name));
    return it == mByName.end() ? nullptr : it->second;
}

std::vector<std::string> CommandMap::tokenize(const std::string &commandLine, std::vector<size_t> *starts,
                                              bool *endsInToken) {
    std::vector<std::string> tokens;
    std::string token;
    bool quoted = false;
    bool escaped = false;
    bool started = false;
    int selectorDepth = 0;
    size_t start = 0;

    for (size_t index = 0; index < commandLine.size(); index++) {
        const char character = commandLine[index];
        if (!started)
            start = index;

        if (escaped) {
            token.push_back(character);
            escaped = false;
            continue;
        }

        if (quoted && character == '\\') {
            if (selectorDepth > 0)
                token.push_back(character);
            escaped = true;
            continue;
        }

        if (character == '"') {
            if (selectorDepth > 0)
                token.push_back(character);
            quoted = !quoted;
            started = true;
            continue;
        }

        if (!quoted && !token.empty() && token[0] == '@') {
            if (character == '[')
                ++selectorDepth;
            else if (character == ']' && selectorDepth > 0)
                --selectorDepth;
        }

        if (!quoted && std::isspace((unsigned char) character)) {
            if (selectorDepth > 0)
                continue;

            if (started) {
                tokens.push_back(token);
                if (starts != nullptr)
                    starts->push_back(start);
                token.clear();
                started = false;
            }
            continue;
        }

        token.push_back(character);
        started = true;
    }

    if (endsInToken != nullptr)
        *endsInToken = started;
    if (started) {
        tokens.push_back(token);
        if (starts != nullptr)
            starts->push_back(start);
    }

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

    std::vector<std::string> tokens = tokenize(line);
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

std::vector<std::string> CommandMap::complete(const std::string &line, const std::vector<std::string> &playerNames,
                                              size_t &replacementStart) const {
    std::vector<size_t> starts;
    bool endsInToken = false;
    auto tokens = tokenize(line, &starts, &endsInToken);
    const bool newArgument = !endsInToken;
    replacementStart = newArgument || starts.empty() ? line.size() : starts.back();
    std::string prefix;
    if (!newArgument && !tokens.empty()) {
        prefix = std::move(tokens.back());
        tokens.pop_back();
    }

    if (tokens.empty() && prefix.starts_with('/')) {
        prefix.erase(0, 1);
        ++replacementStart;
    }
    const std::string lowered = toLowerCase(prefix);
    const auto printable = [](const std::string &value) {
        return std::none_of(value.begin(), value.end(), [](unsigned char byte) {
            return byte < 32 || byte == 127;
        });
    };
    std::vector<std::string> results;
    auto collect = [&](const std::string &value) {
        if (printable(value) && toLowerCase(value).starts_with(lowered))
            results.push_back(value);
    };

    if (tokens.empty()) {
        for (const auto &[name, command]: mByName)
            collect(name);
    } else {
        const std::string &name = tokens.front();
        const Command *command = getCommand(name.starts_with('/') ? name.substr(1) : name);
        if (command == nullptr)
            return {};
        for (const auto &overload: command->getOverloads()) {
            size_t token = 1;
            for (const auto &parameter: overload.mParameters) {
                const bool position = parameter.mHasType
                                      && (parameter.mType == CommandParamType::Position
                                          || parameter.mType == CommandParamType::BlockPosition);
                const bool identifier = parameter.mHasEnumData
                                        && (parameter.mEnumData.mName == "Item" || parameter.mEnumData.mName == "Block");
                if (token >= tokens.size()) {
                    if (parameter.mHasEnumData) {
                        for (const auto &value: parameter.mEnumData.mValues) {
                            collect(value);
                            if (identifier && lowered.starts_with("minecraft:") && value.find(':') == std::string::npos)
                                collect("minecraft:" + value);
                        }
                    }
                    if (parameter.mHasType && (parameter.mType == CommandParamType::Target
                                               || parameter.mType == CommandParamType::WildcardTarget)) {
                        for (const auto &player: playerNames) {
                            if (!printable(player) || !toLowerCase(player).starts_with(lowered))
                                continue;
                            if (player.find_first_of(" \t\"\\") == std::string::npos) {
                                results.push_back(player);
                                continue;
                            }
                            std::string quoted = "\"";
                            for (const char byte: player) {
                                if (byte == '"' || byte == '\\')
                                    quoted += '\\';
                                quoted += byte;
                            }
                            results.push_back(quoted + '"');
                        }
                        for (const char *selector: {"@a", "@e", "@p", "@r", "@s"})
                            collect(selector);
                    }
                    break;
                }
                if (parameter.mHasEnumData && !identifier) {
                    const auto &values = parameter.mEnumData.mValues;
                    if (std::find(values.begin(), values.end(), tokens[token]) == values.end())
                        break;
                }
                token += position ? 3 : 1;
                if (token > tokens.size())
                    break;
            }
        }
    }

    std::sort(results.begin(), results.end());
    results.erase(std::unique(results.begin(), results.end()), results.end());
    return results;
}
