#pragma once

class BookEditPacket;
class ServerNetworkHandler;
class ServerPlayer;

// the server side of the book and quill
class BookHandler {
public:
    // edits the writable book in the given inventory slot; signing turns it into a written book
    static void handleEdit(ServerNetworkHandler &owner, ServerPlayer &player, const BookEditPacket &packet);
};
