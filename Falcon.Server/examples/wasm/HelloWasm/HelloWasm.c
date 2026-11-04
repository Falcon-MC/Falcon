#include "falcon_wasm.h"

static void welcome(void) {
    falcon_player player;
    if (falcon_event_player(&player))
        falcon_send(player, "Welcome! This message comes from WebAssembly.");
}

static bool hello(int argc, const char *const *argv) {
    falcon_reply("Hello from WebAssembly!");
    return true;
}

bool falcon_enable(void) {
    falcon_on(FALCON_EVENT_PLAYER_JOIN, welcome);
    falcon_command("hellowasm", "Answers from a WebAssembly plugin", hello);
    falcon_log("HelloWasm is enabled");
    return true;
}
