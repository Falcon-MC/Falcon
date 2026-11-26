#pragma once

#include "Protocol/Packets/NpcDialoguePacket.h"

#include <string>

class NpcActor;
class NpcRequestPacket;
class ServerNetworkHandler;
class ServerPlayer;
struct NpcAction;
struct NpcScene;

// the server side of the npc screen: opening it, running its buttons and the editor's changes
class NpcHandler {
public:
    // a scene's text, name and buttons; none: empty strings, the client shows the npc's own
    static NpcDialoguePacket makeDialogue(const NpcActor &npc, const NpcScene *scene);

    // false when the scene is named but no pack holds it: nothing opens
    static bool openDialogue(ServerNetworkHandler &owner, NpcActor &npc, ServerPlayer &player,
                             const std::string &sceneName);

    // what the game lets edit an npc: an operator in creative
    static bool canEdit(const ServerPlayer &player);

    // horizontally within the server's tick distance, in the same dimension
    static bool isInRange(ServerNetworkHandler &owner, const NpcActor &npc, const ServerPlayer &player);

    static void handleRequest(ServerNetworkHandler &owner, ServerPlayer &player, const NpcRequestPacket &packet);

    // the npc runs the action's commands for the player; a url action runs nothing
    static void runAction(ServerNetworkHandler &owner, NpcActor &npc, ServerPlayer &player, const NpcAction &action);
};
