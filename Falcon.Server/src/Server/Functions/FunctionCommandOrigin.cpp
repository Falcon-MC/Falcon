#include "Server/Functions/FunctionCommandOrigin.h"

#include "Falcon/Plugin/PluginPermissions.h"

bool FunctionCommandOrigin::canExecuteCommand(const Command &command) const {
    if (mPermissions != nullptr && mGroupPlayer != nullptr) {
        const auto group = mPermissions->commandPermission(*mGroupPlayer, command);
        if (group.has_value())
            return *group;
    }
    return CommandOrigin::canExecuteCommand(command);
}
