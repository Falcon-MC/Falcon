#pragma once

class BlockState;

namespace BlockUpdates {
    // no update does anything to that state: a block of the plain class, in none of the systems
    bool ignores(const BlockState &state);
}
