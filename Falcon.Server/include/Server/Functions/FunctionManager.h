#pragma once

#include "Server/Functions/FunctionCommandOrigin.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class CommandMap;
class PluginPermissions;
struct BehaviorPack;

// the .mcfunction files of the behavior packs, and the stack their commands run from
class FunctionManager {
public:
    struct Function {
        std::vector<std::string> mCommands;
        // a line named no command: the game keeps the function, refuses to run it
        bool mFailed = false;
    };

    // every pack's functions/ folder and tick.json; what does not load is logged, as the game does
    void load(const std::vector<BehaviorPack> &packs, const CommandMap &commands);

    void clear();

    // what decides the commands a player's function may run; set once the plugins exist
    void setPermissions(const PluginPermissions *permissions) {
        mPermissions = permissions;
    }

    const Function *find(const std::string &name) const;

    size_t getFunctionCount() const {
        return mFunctions.size();
    }

    const std::vector<std::string> &getTickFunctions() const {
        return mTickFunctions;
    }

    bool isRunning() const {
        return mRunning;
    }

    // runs it to the end or to the limit of entries; the entries run come back, a nested function counted
    // as one besides its commands
    int run(const Function &function, CommandOrigin &caller, CommandMap &commands, int limit);

    // called from a command the stack runs: the function's commands go on top, run next
    void push(const Function &function, CommandOrigin &caller);

    // the functions tick.json lists, one run each
    void tick(CommandOrigin &caller, CommandMap &commands, int limit);

    // [0-9A-Za-z()-./_]; the first character outside them, or 0
    static char invalidCharacter(const std::string &name);

private:
    struct Entry {
        const std::string *mCommand;
        FunctionCommandOrigin *mOrigin;
    };

    std::unordered_map<std::string, Function> mFunctions;
    std::vector<std::string> mTickFunctions;
    std::vector<Entry> mStack;
    std::vector<std::unique_ptr<FunctionCommandOrigin>> mOrigins;
    const PluginPermissions *mPermissions = nullptr;
    bool mRunning = false;
    int mExecuted = 0;
    int mLimit = 0;
};
