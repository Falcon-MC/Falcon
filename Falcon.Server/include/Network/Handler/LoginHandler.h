#pragma once

#include "Core/Utility/UUID.h"
#include "Protocol/Types/PlayerAbilityData.h"

#include <string>

class ServerNetworkHandler;
class ServerPlayer;
class NetworkIdentifier;
class RequestNetworkSettingsPacket;
class LoginPacket;
class ResourcePackClientResponsePacket;
class ResourcePackChunkRequestPacket;

class LoginHandler {
public:
    static void handleRequestNetworkSettings(ServerNetworkHandler &owner, const NetworkIdentifier &id,
                                             const RequestNetworkSettingsPacket &packet);

    static void handleLogin(ServerNetworkHandler &owner, const NetworkIdentifier &id, ServerPlayer &player,
                            const LoginPacket &packet);

    static void handleClientToServerHandshake(ServerNetworkHandler &owner, const NetworkIdentifier &id,
                                              ServerPlayer &player);

    static void startEncryption(ServerNetworkHandler &owner, const NetworkIdentifier &id, ServerPlayer &player,
                                const std::string &clientPublicKey);

    static void completeLogin(ServerNetworkHandler &owner, const NetworkIdentifier &id, ServerPlayer &player);

    static void handleResourcePackClientResponse(ServerNetworkHandler &owner, const NetworkIdentifier &id,
                                                 ServerPlayer &player,
                                                 const ResourcePackClientResponsePacket &packet);

    static void handleResourcePackChunkRequest(ServerNetworkHandler &owner, const NetworkIdentifier &id,
                                               const ResourcePackChunkRequestPacket &packet);

    static void handleSetLocalPlayerAsInitialized(ServerNetworkHandler &owner, ServerPlayer &player);

    static void sendStartGame(ServerNetworkHandler &owner, ServerPlayer &player);

    static void sendAbilities(ServerNetworkHandler &owner, ServerPlayer &player);

    /** The abilities a player has, as sent to themselves and to the players who see them. */
    static PlayerAbilityData buildAbilities(ServerNetworkHandler &owner, const ServerPlayer &player);

    /** The UUID that identifies the player in the player list, which AddPlayer must reuse. */
    static Uuid playerListUuid(const ServerPlayer &player);

    static void sendBiomeDefinitions(ServerNetworkHandler &owner, ServerPlayer &player);

    static void sendItemComponents(ServerNetworkHandler &owner, ServerPlayer &player);

    static void sendActorIdentifiers(ServerNetworkHandler &owner, ServerPlayer &player);

    static void sendCameraPresets(ServerNetworkHandler &owner, ServerPlayer &player);

    static void buildCraftingData(ServerNetworkHandler &owner);

    static void sendCraftingData(ServerNetworkHandler &owner, ServerPlayer &player);

    static void buildCreativeContent(ServerNetworkHandler &owner);

    static void sendCreativeContent(ServerNetworkHandler &owner, ServerPlayer &player);

    static void sendAttributes(ServerNetworkHandler &owner, ServerPlayer &player);

    static void sendAvailableCommands(ServerNetworkHandler &owner, ServerPlayer &player);

    static void addToPlayerList(ServerNetworkHandler &owner, ServerPlayer &player);

    static void removeFromPlayerList(ServerNetworkHandler &owner, ServerPlayer &player);

    static void checkTerrainReady(ServerNetworkHandler &owner, ServerPlayer &player);

    static void registerVanillaDefinitions(ServerNetworkHandler &owner);
};
