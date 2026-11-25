#include "Protocol/PacketCodecContext.h"

#include "Protocol/Codec/ProtocolCodecRegistry.h"

#include <utility>

PacketCodecContext::PacketCodecContext(const BlockDefinitionRegistry &blockDefinitions,
                                       const ItemDefinitionRegistry &itemDefinitions)
        : mBlockDefinitions(blockDefinitions), mItemDefinitions(itemDefinitions) {}

PacketCodecContext::PacketCodecContext(const BlockDefinitionRegistry &blockDefinitions,
                                       const ItemDefinitionRegistry &itemDefinitions,
                                       std::shared_ptr<const ProtocolCodec> codec)
        : mBlockDefinitions(blockDefinitions), mItemDefinitions(itemDefinitions), mCodec(std::move(codec)) {}

const ProtocolCodec &PacketCodecContext::getCodec() const {
    return mCodec != nullptr ? *mCodec : ProtocolCodecRegistry::instance().getDefault();
}

const ProtocolCapabilities &PacketCodecContext::getCapabilities() const {
    return getCodec().getCapabilities();
}

void PacketCodecContext::putPresence(BinaryStream &stream, bool present) const {
    if (getCapabilities().mDoubledPresence)
        stream.putBool(true);
    stream.putBool(present);
}

void PacketCodecContext::setItemNetworkIds(std::shared_ptr<const ItemNetworkIdMap> itemNetworkIds) {
    mItemNetworkIds = std::move(itemNetworkIds);
}

int32_t PacketCodecContext::toNetworkItemId(int32_t itemId) const {
    return mItemNetworkIds == nullptr ? itemId : mItemNetworkIds->toClient(itemId);
}

int32_t PacketCodecContext::fromNetworkItemId(int32_t networkId) const {
    return mItemNetworkIds == nullptr ? networkId : mItemNetworkIds->toCurrent(networkId);
}

void PacketCodecContext::setBlockNetworkIds(std::shared_ptr<const BlockNetworkIdMap> blockNetworkIds) {
    mBlockNetworkIds = std::move(blockNetworkIds);
}

int32_t PacketCodecContext::toNetworkBlockId(int32_t blockId) const {
    return mBlockNetworkIds == nullptr ? blockId : mBlockNetworkIds->toClient(blockId);
}

int32_t PacketCodecContext::fromNetworkBlockId(int32_t networkId) const {
    return mBlockNetworkIds == nullptr ? networkId : mBlockNetworkIds->toCurrent(networkId);
}

bool PacketCodecContext::getPresence(ReadOnlyBinaryStream &stream) const {
    if (getCapabilities().mDoubledPresence && !stream.getBool())
        return false;
    return stream.getBool();
}
