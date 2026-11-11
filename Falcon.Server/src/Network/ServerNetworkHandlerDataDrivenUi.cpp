#include "Network/ServerNetworkHandler.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "Protocol/Packets/ClientboundDataDrivenUICloseScreenPacket.h"
#include "Protocol/Packets/ClientboundDataDrivenUIShowScreenPacket.h"
#include "Protocol/Packets/ClientboundDataStorePacket.h"
#include "Protocol/Packets/ServerboundDataDrivenScreenClosedPacket.h"
#include "Protocol/Packets/ServerboundDataStorePacket.h"

namespace {
    const char *DATA_STORE_NAME = "minecraft";

    DataStoreChangeValue *childOf(DataStoreChangeValue &value, const std::string &key) {
        if (value.mType != DataStoreChangeValueType::Map)
            return nullptr;
        for (std::pair<std::string, DataStoreChangeValue> &entry: value.mMapValue) {
            if (entry.first == key)
                return &entry.second;
        }
        return nullptr;
    }

    // "layout[3].onClick": a bracket index on an object looks up the decimal key, as the game's path parser does
    DataStoreChangeValue *resolvePath(DataStoreChangeValue &root, const std::string &path) {
        DataStoreChangeValue *current = &root;
        size_t i = 0;
        while (i < path.size() && current != nullptr) {
            if (path[i] == '[') {
                const size_t end = path.find(']', i);
                if (end == std::string::npos || end == i + 1 || end - i > 10)
                    return nullptr;
                uint32_t index = 0;
                for (size_t c = i + 1; c < end; c++) {
                    if (path[c] < '0' || path[c] > '9')
                        return nullptr;
                    index = index * 10 + (uint32_t) (path[c] - '0');
                }
                if (current->mType == DataStoreChangeValueType::Array)
                    current = index < current->mArrayValue.size() ? &current->mArrayValue[index] : nullptr;
                else
                    current = childOf(*current, std::to_string(index));
                i = end + 1;
                if (i < path.size() && path[i] == '.')
                    i++;
                continue;
            }
            size_t end = path.find_first_of(".[", i);
            if (end == std::string::npos)
                end = path.size();
            if (end == i)
                return nullptr;
            current = childOf(*current, path.substr(i, end - i));
            i = end < path.size() && path[end] == '.' ? end + 1 : end;
        }
        return current;
    }

    // the parser takes "a.3" and "a[3]" alike on an object: one spelling for the writable lookup
    std::string canonicalPath(const std::string &path) {
        std::string out;
        size_t i = 0;
        while (i < path.size()) {
            size_t end = path.find_first_of(".[", i + 1);
            if (end == std::string::npos)
                end = path.size();
            std::string segment = path.substr(i, end - i);
            if (segment.size() > 1 && segment[0] == '.'
                && segment.find_first_not_of("0123456789", 1) == std::string::npos)
                segment = "[" + segment.substr(1) + "]";
            out += segment;
            i = end;
        }
        return out;
    }

    // a number lands on a double, as the store keeps it (sub_1468F7010)
    bool assignScalar(DataStoreChangeValue &leaf, const DataStoreScalar &scalar) {
        switch (scalar.mType) {
            case DataStoreScalarType::Number:
                if (leaf.mType != DataStoreChangeValueType::Double)
                    return false;
                leaf.mDoubleValue = scalar.mNumberValue;
                return true;
            case DataStoreScalarType::Bool:
                if (leaf.mType != DataStoreChangeValueType::Bool)
                    return false;
                leaf.mBoolValue = scalar.mBoolValue;
                return true;
            case DataStoreScalarType::String:
                if (leaf.mType != DataStoreChangeValueType::String)
                    return false;
                leaf.mStringValue = scalar.mStringValue;
                return true;
            default:
                return false;
        }
    }

    bool scalarOf(const DataStoreChangeValue &leaf, DataStoreScalar &scalar) {
        switch (leaf.mType) {
            case DataStoreChangeValueType::Double:
                scalar.mType = DataStoreScalarType::Number;
                scalar.mNumberValue = leaf.mDoubleValue;
                return true;
            case DataStoreChangeValueType::Bool:
                scalar.mType = DataStoreScalarType::Bool;
                scalar.mBoolValue = leaf.mBoolValue;
                return true;
            case DataStoreChangeValueType::String:
                scalar.mType = DataStoreScalarType::String;
                scalar.mStringValue = leaf.mStringValue;
                return true;
            default:
                return false;
        }
    }
}

int32_t ServerNetworkHandler::showDataDrivenScreen(ServerPlayer &player, const std::string &screenId,
                                                   const std::string &propertyPrefix, DataStoreChangeValue value,
                                                   std::vector<std::string> writable,
                                                   DataDrivenScreenCallbacks callbacks) {
    if (!player.isSpawned())
        return 0;

    const int32_t formId = mNextDataDrivenScreenId++;
    const NetworkIdentifier &id = player.getNetworkIdentifier();
    DataDrivenScreen &screen = mDataDrivenScreens[id][formId];
    screen.mProperty = propertyPrefix + std::to_string(formId);
    screen.mValue = std::move(value);
    screen.mUpdateCount = 1;
    screen.mPathUpdateCounts.clear();
    screen.mWritable = std::unordered_set<std::string>(writable.begin(), writable.end());
    screen.mCallbacks = std::move(callbacks);

    // the data first: the screen binds to the property as soon as it opens
    _sendDataStoreChange(id, screen.mProperty, screen.mUpdateCount, screen.mValue);

    ClientboundDataDrivenUIShowScreenPacket show;
    show.mScreenId = screenId;
    show.mFormId = formId;
    show.mHasDataInstanceId = true;
    show.mDataInstanceId = formId;
    mNetworkHandler->send(id, show, mCodecContext);
    return formId;
}

void ServerNetworkHandler::updateDataDrivenScreen(ServerPlayer &player, int32_t formId, DataStoreChangeValue value,
                                                  std::vector<std::string> writable) {
    auto playerScreens = mDataDrivenScreens.find(player.getNetworkIdentifier());
    if (playerScreens == mDataDrivenScreens.end())
        return;
    auto it = playerScreens->second.find(formId);
    if (it == playerScreens->second.end())
        return;

    DataDrivenScreen &screen = it->second;
    screen.mValue = std::move(value);
    screen.mUpdateCount++;
    screen.mPathUpdateCounts.clear();
    screen.mWritable = std::unordered_set<std::string>(writable.begin(), writable.end());
    _sendDataStoreChange(playerScreens->first, screen.mProperty, screen.mUpdateCount, screen.mValue);
}

void ServerNetworkHandler::closeDataDrivenScreen(ServerPlayer &player, int32_t formId) {
    if (!player.isSpawned())
        return;

    ClientboundDataDrivenUICloseScreenPacket close;
    close.mHasFormId = true;
    close.mFormId = formId;
    mNetworkHandler->send(player.getNetworkIdentifier(), close, mCodecContext);
}

void ServerNetworkHandler::_sendDataStoreChange(const NetworkIdentifier &id, const std::string &property,
                                                int32_t updateCount, const DataStoreChangeValue &value) {
    ClientboundDataStorePacket packet;
    DataStoreAction action;
    action.mType = DataStoreActionType::Change;
    action.mChange.mDataStoreName = DATA_STORE_NAME;
    action.mChange.mProperty = property;
    action.mChange.mUpdateCount = updateCount;
    action.mChange.mNewValue = value;
    packet.mUpdates.push_back(std::move(action));
    mNetworkHandler->send(id, packet, mCodecContext);
}

void ServerNetworkHandler::_endDataDrivenScreens(const NetworkIdentifier &id, DataDrivenScreenOutcome outcome) {
    auto playerScreens = mDataDrivenScreens.find(id);
    if (playerScreens == mDataDrivenScreens.end())
        return;

    std::unordered_map<int32_t, DataDrivenScreen> screens = std::move(playerScreens->second);
    mDataDrivenScreens.erase(playerScreens);
    for (auto &entry: screens) {
        if (entry.second.mCallbacks.mOnClose)
            entry.second.mCallbacks.mOnClose(outcome);
    }
}

// the server side of sub_1468FBE80: the property count first, then the path count, which must be one ahead
void ServerNetworkHandler::handle(const NetworkIdentifier &id, const ServerboundDataStorePacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned() || !_allowPacket(id, RateLimitedPacket::FormResponse))
        return;

    const DataStoreUpdate &update = packet.mUpdate;
    if (update.mDataStoreName != DATA_STORE_NAME)
        return;

    auto playerScreens = mDataDrivenScreens.find(id);
    if (playerScreens == mDataDrivenScreens.end())
        return;

    DataDrivenScreen *screen = nullptr;
    for (auto &entry: playerScreens->second) {
        if (entry.second.mProperty == update.mProperty) {
            screen = &entry.second;
            break;
        }
    }
    // a path the server never opened: the game logs and drops it
    const std::string path = canonicalPath(update.mPath);
    if (screen == nullptr || screen->mWritable.find(path) == screen->mWritable.end())
        return;

    if (screen->mUpdateCount > update.mUpdateCount) {
        screen->mUpdateCount++;
        screen->mPathUpdateCounts.clear();
        _sendDataStoreChange(id, screen->mProperty, screen->mUpdateCount, screen->mValue);
        return;
    }
    if (screen->mUpdateCount < update.mUpdateCount)
        return;

    DataStoreChangeValue *leaf = resolvePath(screen->mValue, path);
    if (leaf == nullptr)
        return;

    int32_t &pathCount = screen->mPathUpdateCounts[path];
    if (pathCount + 1 == update.mPathUpdateCount) {
        if (!assignScalar(*leaf, update.mData))
            return;
        pathCount++;
        DataStoreScalar accepted;
        scalarOf(*leaf, accepted);
        std::function<void(const std::string &, const DataStoreScalar &)> onWrite = screen->mCallbacks.mOnWrite;
        if (onWrite)
            onWrite(path, accepted);
        return;
    }
    if (pathCount < update.mPathUpdateCount)
        return;

    // the client wrote over a newer server value: send that value back with the next path count
    DataStoreScalar current;
    if (!scalarOf(*leaf, current))
        return;
    pathCount++;

    ClientboundDataStorePacket reply;
    DataStoreAction action;
    action.mType = DataStoreActionType::Update;
    action.mUpdate.mDataStoreName = DATA_STORE_NAME;
    action.mUpdate.mProperty = screen->mProperty;
    action.mUpdate.mPath = update.mPath;
    action.mUpdate.mData = current;
    action.mUpdate.mUpdateCount = screen->mUpdateCount;
    action.mUpdate.mPathUpdateCount = pathCount;
    reply.mUpdates.push_back(std::move(action));
    mNetworkHandler->send(id, reply, mCodecContext);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const ServerboundDataDrivenScreenClosedPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned() || !_allowPacket(id, RateLimitedPacket::FormResponse))
        return;

    auto playerScreens = mDataDrivenScreens.find(id);
    if (playerScreens == mDataDrivenScreens.end())
        return;
    auto it = playerScreens->second.find(packet.mFormId);
    if (it == playerScreens->second.end())
        return;

    DataDrivenScreenCallbacks callbacks = std::move(it->second.mCallbacks);
    playerScreens->second.erase(it);
    if (playerScreens->second.empty())
        mDataDrivenScreens.erase(playerScreens);

    if (callbacks.mOnClose)
        callbacks.mOnClose((DataDrivenScreenOutcome) packet.mCloseReason);
}
