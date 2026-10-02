#include "Actor/ServerPlayer.h"
#include "Command/CommandOrigin.h"
#include "Plugin/PluginApiHelpers.h"
#include "Plugin/PluginManager.h"
#include "Plugin/PluginPermissions.h"
#include "Plugin/PluginServerApi.h"

using namespace PluginApiHelpers;

namespace {
    PluginPermissions &permissions() {
        return PluginManager::getInstance().getPermissions();
    }

    int registerPermission(FalconPlugin *source, const char *node, FalconPermissionDefault defaultValue) {
        if (source == nullptr || node == nullptr)
            return 0;
        return permissions().registerPermission(node, defaultValue) ? 1 : 0;
    }

    int playerHasPermission(FalconPlayer *target, const char *node) {
        if (target == nullptr || node == nullptr)
            return 0;
        return permissions().hasPermission(*player(target), node) ? 1 : 0;
    }

    void playerSetPermission(FalconPlugin *source, FalconPlayer *target, const char *node, int value) {
        if (source == nullptr || target == nullptr || node == nullptr)
            return;
        permissions().setPermission(*plugin(source), *player(target), node, value != 0);
    }

    void playerUnsetPermission(FalconPlugin *source, FalconPlayer *target, const char *node) {
        if (source == nullptr || target == nullptr || node == nullptr)
            return;
        permissions().unsetPermission(*plugin(source), *player(target), node);
    }

    int senderHasPermission(FalconCommandSender *source, const char *node) {
        if (source == nullptr || node == nullptr)
            return 0;

        CommandOrigin *origin = sender(source);
        ServerPlayer *target = origin->asPlayer();
        if (target != nullptr)
            return permissions().hasPermission(*target, node) ? 1 : 0;
        return (int) origin->getCommandPermission() >= (int) CommandPermission::GameDirectors ? 1 : 0;
    }
}

void PluginServerApi::fillPermissions(FalconServerApi &api) {
    api.registerPermission = &registerPermission;
    api.playerHasPermission = &playerHasPermission;
    api.playerSetPermission = &playerSetPermission;
    api.playerUnsetPermission = &playerUnsetPermission;
    api.senderHasPermission = &senderHasPermission;
}
