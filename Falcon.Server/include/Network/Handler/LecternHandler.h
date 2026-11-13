#pragma once

class LecternUpdatePacket;
class ServerNetworkHandler;
class ServerPlayer;

// the server side of the lectern window
class LecternHandler {
public:
    // turns the page of a lectern in reach that holds a book
    static void handleUpdate(ServerNetworkHandler &owner, ServerPlayer &player, const LecternUpdatePacket &packet);
};
