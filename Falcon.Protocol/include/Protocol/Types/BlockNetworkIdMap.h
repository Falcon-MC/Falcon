#pragma once

#include <cstdint>
#include <unordered_map>

/**
 * Translates block network IDs, which are state hashes, for a client of another version. Only the states that
 * version lacks are listed; every other hash is the same on both sides.
 */
class BlockNetworkIdMap {
public:
    /**
     * Shows a current state as one the client knows. A client state standing for several current ones reads back
     * as the first mapped to it.
     */
    void add(int32_t currentId, int32_t clientId);

    int32_t toClient(int32_t currentId) const;

    int32_t toCurrent(int32_t clientId) const;

private:
    std::unordered_map<int32_t, int32_t> mToClient;
    std::unordered_map<int32_t, int32_t> mToCurrent;
};
