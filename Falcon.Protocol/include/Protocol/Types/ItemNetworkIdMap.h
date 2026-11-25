#pragma once

#include <cstdint>
#include <unordered_map>

/**
 * Translates the item network IDs of the current protocol into those of an older client and back. Each version
 * numbers its items differently, while the server keeps one set of IDs for its own items.
 */
class ItemNetworkIdMap {
public:
    /**
     * Maps an item the client knows. Several current items may share one client ID when an item the client lacks
     * is shown as another one; the client ID then translates back to the first item mapped to it.
     */
    void add(int32_t currentId, int32_t clientId);

    /**
     * The client ID of a current item, or 0 when the client has no item to show for it.
     */
    int32_t toClient(int32_t currentId) const;

    /**
     * The current ID of an item the client sent, or 0 when it is unknown.
     */
    int32_t toCurrent(int32_t clientId) const;

    bool knows(int32_t currentId) const;

private:
    std::unordered_map<int32_t, int32_t> mToClient;
    std::unordered_map<int32_t, int32_t> mToCurrent;
};
