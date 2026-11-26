#include "Network/Handler/NpcHandler.h"

#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/NpcRequestPacket.h"
#include "Server/Commands/NpcCommandOrigin.h"
#include "Server/PropertiesSettings.h"
#include "World/Actor/Misc/NpcActor.h"
#include "World/Actor/Misc/NpcSceneRegistry.h"
#include "World/Actor/Player/ServerPlayer.h"

NpcDialoguePacket NpcHandler::makeDialogue(const NpcActor &npc, const NpcScene *scene) {
    NpcDialoguePacket packet;
    packet.mUniqueActorId = npc.getUniqueId();
    packet.mAction = NpcDialoguePacket::Action::Open;
    if (scene == nullptr)
        return packet;
    packet.mDialogue = scene->mText;
    packet.mSceneName = scene->mTag;
    packet.mNpcName = scene->mNpcName;
    packet.mActionJson = NpcAction::writeList(scene->mActions);
    return packet;
}

bool NpcHandler::openDialogue(ServerNetworkHandler &owner, NpcActor &npc, ServerPlayer &player,
                              const std::string &sceneName) {
    const NpcScene *scene = NpcSceneRegistry::getInstance().find(sceneName);
    if (!sceneName.empty() && scene == nullptr)
        return false;
    npc.setOpenedScene(player.getUniqueId(), sceneName);
    owner.getNetworkHandler().send(player.getNetworkIdentifier(), makeDialogue(npc, scene), owner.getCodecContext());
    return true;
}

bool NpcHandler::canEdit(const ServerPlayer &player) {
    return player.isOp() && player.isCreative();
}

bool NpcHandler::isInRange(ServerNetworkHandler &owner, const NpcActor &npc, const ServerPlayer &player) {
    if (!npc.sharesLevelWith(player))
        return false;
    const float range = (float) owner.getProperties().getTickDistance() * 16.0f;
    const float dx = npc.getPosition().x - player.getPosition().x;
    const float dz = npc.getPosition().z - player.getPosition().z;
    return dx * dx + dz * dz <= range * range;
}

void NpcHandler::runAction(ServerNetworkHandler &owner, NpcActor &npc, ServerPlayer &player, const NpcAction &action) {
    if (action.mType != NpcAction::Type::Command)
        return;
    // a command may remove the npc, kick the player or edit the actions: both are looked up again each line
    const std::vector<NpcAction::Command> commands = action.mCommands;
    const uint64_t npcId = npc.getRuntimeId();
    const uint64_t playerId = player.getRuntimeId();
    for (const NpcAction::Command &command: commands) {
        NpcActor *current = dynamic_cast<NpcActor *>(owner.getActorByRuntimeId(npcId));
        ServerPlayer *initiator = owner.getPlayerByRuntimeId(playerId);
        if (current == nullptr || current->isDead() || initiator == nullptr)
            return;
        NpcCommandOrigin origin(owner.getLevelFor(*current), *current, *initiator);
        owner.getCommands().dispatch(origin, command.mLine);
    }
}

void NpcHandler::handleRequest(ServerNetworkHandler &owner, ServerPlayer &player, const NpcRequestPacket &packet) {
    NpcActor *npc = dynamic_cast<NpcActor *>(owner.getActorByRuntimeId(packet.mRuntimeActorId));
    if (npc == nullptr || npc->isDead() || !isInRange(owner, *npc, player))
        return;

    using Request = NpcRequestPacket::RequestType;
    const Request request = packet.mRequestType;
    if ((request == Request::SetAction || request == Request::SetName || request == Request::SetSkin
         || request == Request::SetInteractionText)
        && !canEdit(player))
        return;

    switch (request) {
        case Request::SetAction: {
            // the game keeps the edited text and rebuilds the commands from its lines
            std::vector<NpcAction> actions = NpcAction::parseList(packet.mCommand);
            for (NpcAction &action: actions) {
                if (action.mType == NpcAction::Type::Command)
                    action.splitCommands();
            }
            npc->setActions(&owner, std::move(actions));
            return;
        }
        case Request::SetName:
            npc->setName(&owner, packet.mCommand);
            return;
        case Request::SetSkin:
            npc->setSkinIndex(&owner, packet.mActionType);
            return;
        case Request::SetInteractionText:
            npc->setInteractText(&owner, packet.mCommand);
            return;
        case Request::ExecuteCommandAction:
        case Request::ExecuteClosingCommands:
        case Request::ExecuteOpeningCommands:
            break;
        default:
            return;
    }

    // a scene's buttons only for the scene this player was shown: any other tag would run a pack's
    // commands from any npc
    const std::vector<NpcAction> *actions = &npc->getActions();
    if (!packet.mSceneName.empty()) {
        const NpcScene *scene = packet.mSceneName == npc->getOpenedScene(player.getUniqueId())
                                        ? NpcSceneRegistry::getInstance().find(packet.mSceneName)
                                        : nullptr;
        if (scene == nullptr)
            return;
        actions = &scene->mActions;
    }

    // copied: a command may edit the npc's own list or remove the npc
    const std::vector<NpcAction> steps = *actions;
    const uint64_t npcId = npc->getRuntimeId();
    const uint64_t playerId = player.getRuntimeId();
    if (request == Request::ExecuteCommandAction) {
        if (packet.mActionType >= 0 && (size_t) packet.mActionType < steps.size())
            runAction(owner, *npc, player, steps[(size_t) packet.mActionType]);
        return;
    }

    const NpcAction::Mode mode =
            request == Request::ExecuteOpeningCommands ? NpcAction::Mode::OnOpen : NpcAction::Mode::OnClose;
    for (const NpcAction &action: steps) {
        npc = dynamic_cast<NpcActor *>(owner.getActorByRuntimeId(npcId));
        ServerPlayer *initiator = owner.getPlayerByRuntimeId(playerId);
        if (npc == nullptr || initiator == nullptr)
            return;
        if (action.mType == NpcAction::Type::Command && action.mMode == mode)
            runAction(owner, *npc, *initiator, action);
    }
}
