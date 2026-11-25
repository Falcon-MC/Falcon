#include "Protocol/Types/ItemNetworkIdMap.h"

void ItemNetworkIdMap::add(int32_t currentId, int32_t clientId) {
    mToClient[currentId] = clientId;
    mToCurrent.emplace(clientId, currentId);
}

int32_t ItemNetworkIdMap::toClient(int32_t currentId) const {
    const auto it = mToClient.find(currentId);
    return it == mToClient.end() ? 0 : it->second;
}

int32_t ItemNetworkIdMap::toCurrent(int32_t clientId) const {
    const auto it = mToCurrent.find(clientId);
    return it == mToCurrent.end() ? 0 : it->second;
}

bool ItemNetworkIdMap::knows(int32_t currentId) const {
    return mToClient.find(currentId) != mToClient.end();
}
