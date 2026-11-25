#pragma once

#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Protocol/Codec/ProtocolCapabilities.h"
#include "Protocol/Types/BlockDefinitionRegistry.h"
#include "Protocol/Types/BlockNetworkIdMap.h"
#include "Protocol/Types/ItemDefinitionRegistry.h"
#include "Protocol/Types/ItemNetworkIdMap.h"

#include <memory>

class ProtocolCodec;

/**
 * What encoding and decoding a packet depends on for one connection: the protocol codec it speaks and the block
 * and item registries its network IDs refer to.
 */
class PacketCodecContext {
public:
    /**
     * A context for the current protocol.
     */
    PacketCodecContext(const BlockDefinitionRegistry &blockDefinitions,
                       const ItemDefinitionRegistry &itemDefinitions);

    PacketCodecContext(const BlockDefinitionRegistry &blockDefinitions,
                       const ItemDefinitionRegistry &itemDefinitions,
                       std::shared_ptr<const ProtocolCodec> codec);

    const BlockDefinitionRegistry &getBlockDefinitions() const { return mBlockDefinitions; }

    const ItemDefinitionRegistry &getItemDefinitions() const { return mItemDefinitions; }

    const ProtocolCodec &getCodec() const;

    const ProtocolCapabilities &getCapabilities() const;

    /**
     * Writes whether an optional field follows, in the form this version expects.
     */
    void putPresence(BinaryStream &stream, bool present) const;

    bool getPresence(ReadOnlyBinaryStream &stream) const;

    /**
     * Makes this context translate item network IDs for a client of another version.
     */
    void setItemNetworkIds(std::shared_ptr<const ItemNetworkIdMap> itemNetworkIds);

    int32_t toNetworkItemId(int32_t itemId) const;

    int32_t fromNetworkItemId(int32_t networkId) const;

    /**
     * Makes this context translate block network IDs for a client of another version.
     */
    void setBlockNetworkIds(std::shared_ptr<const BlockNetworkIdMap> blockNetworkIds);

    /**
     * The block translation of this context, or nullptr when the client shares the server's block states.
     */
    const BlockNetworkIdMap *getBlockNetworkIds() const { return mBlockNetworkIds.get(); }

    int32_t toNetworkBlockId(int32_t blockId) const;

    int32_t fromNetworkBlockId(int32_t networkId) const;

private:
    const BlockDefinitionRegistry &mBlockDefinitions;
    const ItemDefinitionRegistry &mItemDefinitions;
    std::shared_ptr<const ProtocolCodec> mCodec;
    std::shared_ptr<const ItemNetworkIdMap> mItemNetworkIds;
    std::shared_ptr<const BlockNetworkIdMap> mBlockNetworkIds;
};
