#include "Server/Functions/FunctionManager.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "Scripting/BehaviorPackManager.h"
#include "Server/Commands/CommandMap.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
    const char *const FUNCTIONS_DIRECTORY = "functions";
    const char *const FUNCTION_EXTENSION = ".mcfunction";
    const char *const TICK_FILE = "tick.json";
    const char *const TICK_FIELD = "values";

    std::string trimmed(const std::string &line) {
        const size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            return std::string();
        const size_t last = line.find_last_not_of(" \t\r\n");
        return line.substr(first, last - first + 1);
    }

    bool readFile(const std::filesystem::path &path, std::string &out) {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            return false;
        std::ostringstream text;
        text << file.rdbuf();
        out = text.str();
        return true;
    }

    // the relative path without the extension, with forward slashes: "folder/name"
    std::string functionName(const std::filesystem::path &root, const std::filesystem::path &file) {
        std::filesystem::path relative = std::filesystem::relative(file, root);
        relative.replace_extension();
        return relative.generic_string();
    }
}

void FunctionManager::clear() {
    mFunctions.clear();
    mTickFunctions.clear();
}

void FunctionManager::load(const std::vector<BehaviorPack> &packs, const CommandMap &commands) {
    clear();
    std::vector<std::filesystem::path> roots;
    for (const BehaviorPack &pack: packs) {
        const std::filesystem::path root = std::filesystem::path(pack.mPath) / FUNCTIONS_DIRECTORY;
        std::error_code error;
        if (!std::filesystem::is_directory(root, error))
            continue;
        roots.push_back(root);

        std::vector<std::filesystem::path> files;
        for (std::filesystem::recursive_directory_iterator it(root, error), end; !error && it != end;
             it.increment(error)) {
            if (it->is_regular_file(error) && it->path().extension() == FUNCTION_EXTENSION)
                files.push_back(it->path());
        }
        std::sort(files.begin(), files.end());

        for (const std::filesystem::path &file: files) {
            const std::string name = functionName(root, file);
            std::string text;
            if (!readFile(file, text)) {
                LOG_ERROR(LogAreaID::Server, "Couldn't load file %s in functions directory.", name.c_str());
                continue;
            }

            Function function;
            std::vector<std::string> errors;
            std::istringstream lines(text);
            std::string raw;
            for (int number = 1; std::getline(lines, raw); ++number) {
                const std::string line = trimmed(raw);
                if (line.empty() || line[0] == '#')
                    continue;
                const std::vector<std::string> tokens = CommandMap::tokenize(line);
                if (tokens.empty())
                    continue;
                if (commands.getCommand(tokens[0]) == nullptr) {
                    std::string message = "Error on line " + std::to_string(number) + ": unknown or invalid command '"
                                          + tokens[0] + "'";
                    if (line.rfind("//", 0) == 0)
                        message += " (If you intended to make a comment use '#' instead of '//')";
                    else if (line[0] == '/')
                        message += " (Commands in functions may not begin with '/')";
                    errors.push_back(message);
                    continue;
                }
                function.mCommands.push_back(line);
            }

            if (!errors.empty()) {
                function.mFailed = true;
                std::string report = "Function " + name + " failed to load correctly with error(s):";
                for (const std::string &message: errors)
                    report += "\n" + message;
                LOG_ERROR(LogAreaID::Server, "%s", report.c_str());
            }
            mFunctions[name] = std::move(function);
        }
    }

    // tick.json is read once every function is known: it may list one from another pack
    for (const std::filesystem::path &root: roots) {
        std::string text;
        if (!readFile(root / TICK_FILE, text))
            continue;
        const std::unique_ptr<json::Value> tick = json::parse(text);
        const json::Value *values = tick == nullptr ? nullptr : tick->get(TICK_FIELD);
        if (values == nullptr || !values->isArray()) {
            LOG_ERROR(LogAreaID::Server,
                      "Field '%s' not found in tick.json or was found but is not an array, couldn't load tick "
                      "functions.",
                      TICK_FIELD);
            continue;
        }
        for (const std::unique_ptr<json::Value> &value: values->mArray) {
            const std::string name = value == nullptr ? std::string() : value->string();
            if (mFunctions.find(name) == mFunctions.end()) {
                LOG_ERROR(LogAreaID::Server, "Function listed in tick.json with name %s not found.", name.c_str());
                continue;
            }
            mTickFunctions.push_back(name);
        }
    }
}

const FunctionManager::Function *FunctionManager::find(const std::string &name) const {
    const auto found = mFunctions.find(name);
    return found == mFunctions.end() ? nullptr : &found->second;
}

void FunctionManager::push(const Function &function, CommandOrigin &caller) {
    mOrigins.push_back(std::make_unique<FunctionCommandOrigin>(caller, mPermissions));
    FunctionCommandOrigin *origin = mOrigins.back().get();

    // no more on the stack than entries left to run: the first commands go on top, what lies below is
    // dropped
    const size_t left = (size_t) std::max(0, mLimit - mExecuted);
    const size_t taken = std::min(left, function.mCommands.size());
    for (size_t index = taken; index-- > 0;)
        mStack.push_back(Entry{&function.mCommands[index], origin});
    if (mStack.size() > left)
        mStack.erase(mStack.begin(), mStack.begin() + (std::ptrdiff_t) (mStack.size() - left));
}

int FunctionManager::run(const Function &function, CommandOrigin &caller, CommandMap &commands, int limit) {
    mRunning = true;
    mExecuted = 0;
    mLimit = limit;
    push(function, caller);
    while (!mStack.empty()) {
        const Entry entry = mStack.back();
        mStack.pop_back();
        commands.dispatch(*entry.mOrigin, *entry.mCommand);
        if (++mExecuted >= mLimit) {
            mStack.clear();
            break;
        }
    }
    mRunning = false;
    mOrigins.clear();
    return mExecuted;
}

void FunctionManager::tick(CommandOrigin &caller, CommandMap &commands, int limit) {
    for (const std::string &name: mTickFunctions) {
        const Function *function = find(name);
        if (function != nullptr && !function->mFailed)
            run(*function, caller, commands, limit);
    }
}

char FunctionManager::invalidCharacter(const std::string &name) {
    for (const char character: name) {
        const bool allowed = (character >= '0' && character <= '9') || (character >= 'a' && character <= 'z')
                             || (character >= 'A' && character <= 'Z') || character == '(' || character == ')'
                             || character == '-' || character == '.' || character == '/' || character == '_';
        if (!allowed)
            return character;
    }
    return 0;
}
