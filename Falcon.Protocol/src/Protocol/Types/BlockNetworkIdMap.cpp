#include "Protocol/Types/BlockNetworkIdMap.h"

void BlockNetworkIdMap::add(int32_t currentId, int32_t clientId) {
    mToClient[currentId] = clientId;
    mToCurrent.emplace(clientId, currentId);
}

int32_t BlockNetworkIdMap::toClient(int32_t currentId) const {
    const auto it = mToClient.find(currentId);
    return it == mToClient.end() ? currentId : it->second;
}

int32_t BlockNetworkIdMap::toCurrent(int32_t clientId) const {
    const auto it = mToCurrent.find(clientId);
    return it == mToCurrent.end() ? clientId : it->second;
}
