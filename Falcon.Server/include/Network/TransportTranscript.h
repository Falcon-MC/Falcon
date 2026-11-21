#pragma once

#include <cstddef>

extern "C" {

// what the peer holding this certificate sent while its secure session was set up, as records of one
// byte (set when the server sent it), two bytes of length and the handshake message. Zero when
// nothing is held; a transcript is handed out once
size_t falcon_transport_take(const char *fingerprint, unsigned char *out, size_t capacity);
}
