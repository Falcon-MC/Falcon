#include "Network/ProtocolData.h"

#include "OlderProtocolData.h"

const ProtocolData *ProtocolData::find(int32_t protocol) {
    const ProtocolData *found = nullptr;
    for (const ProtocolData &entry: FalconOlderData::kEntries) {
        if (entry.mProtocol == 0 || entry.mProtocol < protocol)
            continue;

        if (found == nullptr || entry.mProtocol < found->mProtocol)
            found = &entry;
    }
    return found;
}
