#include "Server/Commands/CommandSyntax.h"

#include <algorithm>
#include <cctype>

namespace {
    // the game's parser shows this much of the line on each side of the token it stopped at
    const size_t CONTEXT_CHARACTERS = 10;

    std::string lowered(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
            return (char) std::tolower(character);
        });
        return value;
    }

    bool isInteger(const std::string &token) {
        size_t index = token.size() > 1 && (token[0] == '-' || token[0] == '+') ? 1 : 0;
        if (index >= token.size())
            return false;
        for (; index < token.size(); ++index) {
            if (std::isdigit((unsigned char) token[index]) == 0)
                return false;
        }
        return true;
    }

    bool isNumber(const std::string &token) {
        size_t index = token.size() > 1 && (token[0] == '-' || token[0] == '+') ? 1 : 0;
        bool digits = false;
        bool point = false;
        for (; index < token.size(); ++index) {
            const char character = token[index];
            if (std::isdigit((unsigned char) character) != 0)
                digits = true;
            else if (character == '.' && !point)
                point = true;
            else
                return false;
        }
        return digits;
    }

    // ~, ^, either followed by a number, or a plain number
    bool isCoordinate(const std::string &token, bool block) {
        if (token.empty())
            return false;
        if (token[0] == '~' || token[0] == '^')
            return token.size() == 1 || isNumber(token.substr(1));
        return block ? isInteger(token) : isNumber(token);
    }

    // 5, 1.., ..5, 1..5, each side an integer, ! in front to invert
    bool isIntegerRange(const std::string &token) {
        const std::string value = !token.empty() && token[0] == '!' ? token.substr(1) : token;
        const size_t dots = value.find("..");
        if (dots == std::string::npos)
            return isInteger(value);
        const std::string low = value.substr(0, dots);
        const std::string high = value.substr(dots + 2);
        return (!low.empty() || !high.empty()) && (low.empty() || isInteger(low)) && (high.empty() || isInteger(high));
    }

    bool isSelector(const std::string &token) {
        if (token.empty() || token[0] != '@')
            return !token.empty();
        static const char *const SELECTORS[] = {"@a", "@p", "@r", "@e", "@s", "@initiator"};
        for (const char *selector: SELECTORS) {
            const std::string name(selector);
            if (token.compare(0, name.size(), name) != 0)
                continue;
            const std::string rest = token.substr(name.size());
            if (rest.empty() || (rest.front() == '[' && rest.back() == ']'))
                return true;
        }
        return false;
    }

    bool matchesEnum(const CommandEnumData &data, const std::string &token) {
        // add-ons extend these after the overloads are built: the command checks the name itself
        static const char *const OPEN_ENUMS[] = {"Item", "Block", "EntityType", "Entity", "Effect", "Enchant",
                                                 "Particle", "Sound", "Biome", "Feature", "FeatureRule", "Structure"};
        for (const char *open: OPEN_ENUMS) {
            if (data.mName == open)
                return true;
        }
        if (data.mIsSoft || data.mValues.empty())
            return true;
        const std::string value = lowered(token);
        const std::string bare = value.rfind("minecraft:", 0) == 0 ? value.substr(10) : value;
        for (const std::string &candidate: data.mValues) {
            const std::string known = lowered(candidate);
            if (known == value || known == bare
                || (known.rfind("minecraft:", 0) == 0 && known.substr(10) == bare))
                return true;
        }
        return false;
    }

    bool readsRestOfLine(const CommandParamData &parameter) {
        if (!parameter.mHasType || parameter.mHasEnumData)
            return false;
        switch (parameter.mType) {
            case CommandParamType::Message:
            case CommandParamType::RawText:
            case CommandParamType::Json:
            case CommandParamType::Command:
                return true;
            default:
                return false;
        }
    }

    // tokens the parameter takes when it matches, 0 when the token at first does not fit it
    size_t matchParameter(const CommandParamData &parameter, const std::vector<std::string> &tokens, size_t first) {
        const std::string &token = tokens[first];
        if (parameter.mHasEnumData)
            return matchesEnum(parameter.mEnumData, token) ? 1 : 0;
        if (!parameter.mHasType)
            return 1;

        switch (parameter.mType) {
            case CommandParamType::Int:
                return isInteger(token) ? 1 : 0;
            case CommandParamType::Float:
            case CommandParamType::Value:
                return isNumber(token) ? 1 : 0;
            case CommandParamType::WildcardInt:
                return token == "*" || isInteger(token) ? 1 : 0;
            case CommandParamType::Operator: {
                static const char *const OPERATORS[] = {"=", "+=", "-=", "*=", "/=", "%=", "<", ">", "><"};
                return std::any_of(std::begin(OPERATORS), std::end(OPERATORS),
                                   [&](const char *candidate) { return token == candidate; }) ? 1 : 0;
            }
            case CommandParamType::CompareOperator: {
                static const char *const OPERATORS[] = {"<", "<=", "=", ">=", ">"};
                return std::any_of(std::begin(OPERATORS), std::end(OPERATORS),
                                   [&](const char *candidate) { return token == candidate; }) ? 1 : 0;
            }
            case CommandParamType::Target:
                return isSelector(token) ? 1 : 0;
            case CommandParamType::WildcardTarget:
                return token == "*" || isSelector(token) ? 1 : 0;
            case CommandParamType::IntegerRange:
                return isIntegerRange(token) ? 1 : 0;
            case CommandParamType::Position:
            case CommandParamType::BlockPosition: {
                const bool block = parameter.mType == CommandParamType::BlockPosition;
                for (size_t axis = 0; axis < 3; ++axis) {
                    if (first + axis >= tokens.size() || !isCoordinate(tokens[first + axis], block))
                        return 0;
                }
                return 3;
            }
            default:
                return 1;
        }
    }

    // how far one overload reads: the index of the first token it rejects, or of the missing one
    struct Reach {
        bool mAccepted = false;
        size_t mStop = 0;
    };

    Reach reach(const CommandOverloadData &overload, const std::vector<std::string> &tokens) {
        size_t next = 1;
        for (const CommandParamData &parameter: overload.mParameters) {
            if (next >= tokens.size())
                return parameter.mOptional ? Reach{true, next} : Reach{false, next};
            if (readsRestOfLine(parameter))
                return Reach{true, tokens.size()};

            size_t taken = matchParameter(parameter, tokens, next);
            // a position cut short stops at the axis that does not fit
            if (taken == 0 && parameter.mHasType && !parameter.mHasEnumData
                && (parameter.mType == CommandParamType::Position || parameter.mType == CommandParamType::BlockPosition)) {
                const bool block = parameter.mType == CommandParamType::BlockPosition;
                size_t axis = 0;
                while (next + axis < tokens.size() && axis < 3 && isCoordinate(tokens[next + axis], block))
                    ++axis;
                return Reach{false, next + axis};
            }
            if (taken == 0)
                return Reach{false, next};
            next += taken;
        }
        return next >= tokens.size() ? Reach{true, next} : Reach{false, next};
    }
}

CommandSyntaxError CommandSyntax::around(const std::string &line, size_t start, size_t end) {
    start = std::min(start, line.size());
    end = std::clamp(end, start, line.size());
    const size_t from = start > CONTEXT_CHARACTERS ? start - CONTEXT_CHARACTERS : 0;
    const size_t to = std::min(end + CONTEXT_CHARACTERS, line.size());
    return CommandSyntaxError{line.substr(from, start - from), line.substr(start, end - start), line.substr(end, to - end)};
}

std::optional<CommandSyntaxError> CommandSyntax::find(const std::vector<CommandOverloadData> &overloads,
                                                      const std::string &line, const std::vector<std::string> &tokens,
                                                      const std::vector<size_t> &starts,
                                                      const std::vector<size_t> &ends) {
    if (overloads.empty() || tokens.empty() || starts.size() != tokens.size() || ends.size() != tokens.size())
        return std::nullopt;
    // subcommand chains (execute) are checked by the command itself
    for (const CommandOverloadData &overload: overloads) {
        if (overload.mChaining)
            return std::nullopt;
    }

    size_t stop = 0;
    for (const CommandOverloadData &overload: overloads) {
        const Reach result = reach(overload, tokens);
        if (result.mAccepted)
            return std::nullopt;
        stop = std::max(stop, result.mStop);
    }

    if (stop >= tokens.size())
        return around(line, line.size(), line.size());
    return around(line, starts[stop], ends[stop]);
}
