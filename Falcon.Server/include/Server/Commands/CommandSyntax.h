#pragma once

#include "Protocol/Types/CommandData.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

// where a command line stops matching every overload of its command, as the game's parser reports it
struct CommandSyntaxError {
    // up to ten characters of the line on each side of the token, the token itself empty when one is missing
    std::string mBefore;
    std::string mToken;
    std::string mAfter;
};

namespace CommandSyntax {
    // tokens and their [start, end) offsets in line; token 0 is the command name. A parameter at rawIndex or
    // later that reads the rest of the line takes every remaining token. Nothing when some overload accepts
    // the whole line.
    std::optional<CommandSyntaxError> find(const std::vector<CommandOverloadData> &overloads, const std::string &line,
                                           const std::vector<std::string> &tokens, const std::vector<size_t> &starts,
                                           const std::vector<size_t> &ends);

    // the game's commands.generic.syntax parameters: ten characters before, the token, ten after
    CommandSyntaxError around(const std::string &line, size_t start, size_t end);
}
