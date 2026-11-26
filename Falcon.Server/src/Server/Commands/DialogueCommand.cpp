#include "Server/Commands/DialogueCommand.h"

#include "Network/Handler/NpcHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Misc/NpcActor.h"
#include "World/Actor/Misc/NpcSceneRegistry.h"
#include "World/Actor/Player/ServerPlayer.h"

DialogueCommand::DialogueCommand(ServerNetworkHandler &handler)
        : Command("dialogue", "commands.dialogue.description",
                  "/dialogue open <npc> <player> [sceneName] | /dialogue change <npc> <sceneName> [players]"),
          mHandler(handler) {}

std::vector<CommandOverloadData> DialogueCommand::getOverloads() const {
    CommandOverloadData open;
    open.mParameters = {makeEnumParameter("open", "DialogueOpenAction", {"open"}), makePlayerParameter("npc"),
                        makePlayerParameter("player"), makeTypedParameter("sceneName", CommandParamType::String, true)};

    CommandOverloadData change;
    CommandParamData players = makePlayerParameter("players");
    players.mOptional = true;
    change.mParameters = {makeEnumParameter("change", "DialogueChangeAction", {"change"}), makePlayerParameter("npc"),
                          makeTypedParameter("sceneName", CommandParamType::String), players};

    return {open, change};
}

bool DialogueCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.size() < 3 || (arguments[0] != "open" && arguments[0] != "change")) {
        sendUsageError(sender);
        return false;
    }

    // players carry no npc component: only the other entities are looked at
    const std::vector<ServerActor *> npcs = mHandler.resolveActorTargets(sender, arguments[1]);
    if (npcs.empty()) {
        sender.sendTranslation("commands.dialogue.npcNotFound", {});
        return false;
    }
    if (npcs.size() > 1) {
        sender.sendTranslation("commands.dialogue.tooManyNPCs", {});
        return false;
    }
    NpcActor *npc = dynamic_cast<NpcActor *>(npcs.front());
    if (npc == nullptr) {
        sender.sendTranslation("commands.dialogue.noNpcComponent", {});
        return false;
    }

    return arguments[0] == "open" ? _open(sender, *npc, arguments) : _change(sender, *npc, arguments);
}

bool DialogueCommand::_open(CommandOrigin &sender, NpcActor &npc, const std::vector<std::string> &arguments) {
    const std::vector<ServerPlayer *> players = mHandler.resolveTargets(sender, arguments[2]);
    if (players.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    std::vector<ServerPlayer *> opened;
    for (ServerPlayer *player: players) {
        if (!NpcHandler::isInRange(mHandler, npc, *player)) {
            sender.sendTranslation("commands.dialogue.npcNotInRange", {player->getName()});
            continue;
        }
        const std::string &scene = arguments.size() > 3 ? arguments[3] : npc.getSceneFor(player->getUniqueId());
        if (!NpcHandler::openDialogue(mHandler, npc, *player, scene)) {
            sender.sendTranslation("commands.dialogue.invalidScene", {});
            continue;
        }
        opened.push_back(player);
    }
    if (opened.empty())
        return false;
    sender.sendTranslation("commands.dialogue.success", {joinPlayerNames(opened)});
    return true;
}

bool DialogueCommand::_change(CommandOrigin &sender, NpcActor &npc, const std::vector<std::string> &arguments) {
    const std::string &scene = arguments[2];
    if (NpcSceneRegistry::getInstance().find(scene) == nullptr) {
        sender.sendTranslation("commands.dialogue.invalidScene", {});
        return false;
    }

    if (arguments.size() < 4) {
        npc.setSceneName(scene);
        sender.sendTranslation("commands.dialogue.changeSuccess", {});
        return true;
    }

    const std::vector<ServerPlayer *> players = mHandler.resolveTargets(sender, arguments[3]);
    if (players.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }
    for (ServerPlayer *player: players)
        npc.setPlayerScene(player->getUniqueId(), scene);
    sender.sendTranslation("commands.dialogue.changePerPlayerSuccess", {joinPlayerNames(players)});
    return true;
}
