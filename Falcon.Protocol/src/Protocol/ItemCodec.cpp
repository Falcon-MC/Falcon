#include "Protocol/ItemCodec.h"

#include "Core/NBT/NbtIo.h"

namespace {
    const std::string BLOCKING_ID = "minecraft:shield";
    const uint32_t MAX_BLOCK_LIST_ENTRIES = 250;

    uint32_t readBlockListLength(ReadOnlyBinaryStream &stream) {
        const uint32_t length = stream.getLInt();
        if (length > MAX_BLOCK_LIST_ENTRIES)
            throw BinaryDataException("Item block list of " + std::to_string(length) + " entries exceeds "
                                      + std::to_string(MAX_BLOCK_LIST_ENTRIES));
        return length;
    }

    Tag networkItemTag(const ItemStack &item) {
        if (item.mDamage <= 0)
            return item.mTag;

        Tag tag = item.mTag.isCompound() ? item.mTag : Tag::ofCompound();
        tag.putInt("Damage", item.mDamage);
        return tag;
    }

    void writeUserData(BinaryStream &stream, const ItemStack &item) {
        if (!item.mUserData.empty()) {
            stream.putUnsignedVarInt((uint32_t) item.mUserData.size());
            stream.put(item.mUserData);
            return;
        }

        const Tag networkTag = networkItemTag(item);

        BinaryStream userData;
        if (networkTag.getType() != Tag::Type::End) {
            userData.putLShort(0xffff);
            userData.putByte(1);
            NbtIo::writeTag(userData, networkTag, NbtVariant::LittleEndian);
        } else {
            userData.putLShort(0);
        }

        userData.putLInt((uint32_t) item.mCanPlace.size());
        for (const std::string &entry: item.mCanPlace) {
            userData.putLShort((uint16_t) entry.size());
            userData.put(entry);
        }

        userData.putLInt((uint32_t) item.mCanBreak.size());
        for (const std::string &entry: item.mCanBreak) {
            userData.putLShort((uint16_t) entry.size());
            userData.put(entry);
        }

        if (item.mDefinition->getIdentifier() == BLOCKING_ID) {
            userData.putLLong((uint64_t) item.mBlockingTicks);
        }

        stream.putUnsignedVarInt((uint32_t) userData.size());
        stream.put(userData.getBuffer());
    }

    void readBlockList(ReadOnlyBinaryStream &stream, std::vector<std::string> &entries) {
        const uint32_t length = readBlockListLength(stream);
        entries.reserve(length);
        for (uint32_t i = 0; i < length; i++) {
            entries.push_back(stream.get(stream.getLShort()));
        }
    }

    void writeBlockList(BinaryStream &stream, const std::vector<std::string> &entries) {
        stream.putLInt((uint32_t) entries.size());
        for (const std::string &entry: entries) {
            stream.putLShort((uint16_t) entry.size());
            stream.put(entry);
        }
    }

    void readDirectUserData(ReadOnlyBinaryStream &stream, ItemStack &item, bool hasTag) {
        if (hasTag) {
            item.mTag = NbtIo::readTag(stream, NbtVariant::LittleEndian);
        }
        readBlockList(stream, item.mCanPlace);
        readBlockList(stream, item.mCanBreak);
        if (item.mDefinition != nullptr && item.mDefinition->getIdentifier() == BLOCKING_ID) {
            item.mBlockingTicks = stream.getLLong();
        }
    }

    void writeDirectUserData(BinaryStream &stream, const ItemStack &item) {
        const Tag networkTag = networkItemTag(item);
        const bool hasTag = networkTag.getType() != Tag::Type::End;
        stream.putBool(hasTag);
        if (hasTag) {
            NbtIo::writeTag(stream, networkTag, NbtVariant::LittleEndian);
        }
        writeBlockList(stream, item.mCanPlace);
        writeBlockList(stream, item.mCanBreak);
        if (item.mDefinition->getIdentifier() == BLOCKING_ID) {
            stream.putLLong((uint64_t) item.mBlockingTicks);
        }
    }

    void applyNetworkDamage(ItemStack &item) {
        if (item.mTag.isCompound() && item.mTag.get("Damage") != nullptr)
            item.mDamage = item.mTag.getInt("Damage", item.mDamage);
    }
}

void ItemCodec::writeItemInstance(BinaryStream &stream, const PacketCodecContext &context, const ItemStack &item) {
    const int32_t networkId = item.isAir() ? 0 : context.toNetworkItemId(item.mDefinition->getRuntimeId());
    if (networkId == 0) {
        stream.putVarInt(0);
        stream.putLShort(0);
        stream.putUnsignedVarInt((uint32_t) item.mDamage);
        stream.putVarInt(0);
        stream.putString("");
        return;
    }

    stream.putVarInt(networkId);
    stream.putLShort((uint16_t) item.mCount);
    stream.putUnsignedVarInt((uint32_t) item.mDamage);
    stream.putVarInt(item.mBlockDefinition == nullptr
                     ? 0 : context.toNetworkBlockId(item.mBlockDefinition->getRuntimeId()));

    writeUserData(stream, item);
}

ItemStack ItemCodec::readItemInstance(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    int runtimeId = stream.getVarInt();
    if (runtimeId == 0) {
        stream.getLShort();
        stream.getUnsignedVarInt();
        stream.getVarInt();
        stream.get(stream.getUnsignedVarInt());
        return ItemStack::air();
    }

    ItemStack item;
    item.mDefinition = context.getItemDefinitions().getDefinition(context.fromNetworkItemId(runtimeId));
    item.mCount = stream.getLShort();
    item.mDamage = (int) stream.getUnsignedVarInt();

    int blockRuntimeId = stream.getVarInt();
    if (blockRuntimeId != 0) {
        item.mBlockDefinition = context.getBlockDefinitions().getDefinition(context.fromNetworkBlockId(blockRuntimeId));
    }

    uint32_t userDataLength = stream.getUnsignedVarInt();
    std::string userDataBytes = stream.get(userDataLength);
    item.mUserData = userDataBytes;
    if (userDataBytes.empty()) {
        applyNetworkDamage(item);
        return item;
    }

    ReadOnlyBinaryStream userData(userDataBytes);

    uint16_t nbtSize = userData.getLShort();
    if (nbtSize == 0xffff) {
        unsigned char tagCount = userData.getByte();
        if (tagCount != 1) {
            throw BinaryDataException("Expected 1 tag but got " + std::to_string((int) tagCount));
        }
        item.mTag = NbtIo::readTag(userData, NbtVariant::LittleEndian);
    } else if (nbtSize > 0) {
        item.mTag = NbtIo::readTag(userData, NbtVariant::LittleEndian);
    }

    uint32_t canPlaceLength = readBlockListLength(userData);
    item.mCanPlace.reserve(canPlaceLength);
    for (uint32_t i = 0; i < canPlaceLength; i++) {
        item.mCanPlace.push_back(userData.get(userData.getLShort()));
    }

    uint32_t canBreakLength = readBlockListLength(userData);
    item.mCanBreak.reserve(canBreakLength);
    for (uint32_t i = 0; i < canBreakLength; i++) {
        item.mCanBreak.push_back(userData.get(userData.getLShort()));
    }

    if (item.mDefinition != nullptr && item.mDefinition->getIdentifier() == BLOCKING_ID
            && userData.getRemainingLength() >= 8) {
        item.mBlockingTicks = userData.getLLong();
    }

    applyNetworkDamage(item);
    return item;
}

void ItemCodec::writeNetworkItemStackDescriptor(BinaryStream &stream, const PacketCodecContext &context, const ItemStack &item) {
    const int32_t networkId = item.isAir() ? 0 : context.toNetworkItemId(item.mDefinition->getRuntimeId());
    if (networkId == 0) {
        stream.putLShort(0);
        stream.putLShort(0);
        stream.putUnsignedVarInt((uint32_t) item.mDamage);
        stream.putBool(false);
        stream.putUnsignedVarInt(0);
        stream.putString("");
        return;
    }

    stream.putLShort((uint16_t) networkId);
    stream.putLShort((uint16_t) item.mCount);
    stream.putUnsignedVarInt((uint32_t) item.mDamage);

    stream.putBool(item.mUsingNetId);
    if (item.mUsingNetId) {
        stream.putVarInt(item.mNetId);
    }

    stream.putUnsignedVarInt(item.mBlockDefinition == nullptr
                             ? 0 : (uint32_t) context.toNetworkBlockId(item.mBlockDefinition->getRuntimeId()));

    if (item.mDirectUserData) {
        writeDirectUserData(stream, item);
        return;
    }
    writeUserData(stream, item);
}

void ItemCodec::writeRequestItemDescriptor(BinaryStream &stream, const PacketCodecContext &context,
                                           const ItemStack &item) {
    (void) context;
    bool air = item.isAir();
    stream.putUnsignedVarInt(air ? 0 : 1);
    stream.putByte(air ? 0 : 1);
    if (!air) {
        stream.putString(item.mDefinition->getIdentifier());
        stream.putVarInt(item.mDamage);
    }

    stream.putLShort((uint16_t) item.mCount);
    stream.putUnsignedVarInt(air || item.mBlockDefinition == nullptr
                             ? 0 : (uint32_t) context.toNetworkBlockId(item.mBlockDefinition->getRuntimeId()));

    if (air) {
        stream.putString("");
        return;
    }

    BinaryStream userData;
    if (item.mTag.getType() != Tag::Type::End) {
        userData.putLShort(0xffff);
        userData.putByte(1);
        NbtIo::writeTag(userData, item.mTag, NbtVariant::LittleEndian);
    } else {
        userData.putLShort(0);
    }

    userData.putLInt((uint32_t) item.mCanPlace.size());
    for (const std::string &entry: item.mCanPlace) {
        userData.putLShort((uint16_t) entry.size());
        userData.put(entry);
    }

    userData.putLInt((uint32_t) item.mCanBreak.size());
    for (const std::string &entry: item.mCanBreak) {
        userData.putLShort((uint16_t) entry.size());
        userData.put(entry);
    }

    if (item.mDefinition->getIdentifier() == BLOCKING_ID) {
        userData.putLLong((uint64_t) item.mBlockingTicks);
    }

    stream.putUnsignedVarInt((uint32_t) userData.size());
    stream.put(userData.getBuffer());
}

ItemStack ItemCodec::readRequestItemDescriptor(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    int32_t type = (int32_t) stream.getUnsignedVarInt();
    stream.getByte();

    ItemStack item;
    if (type != 0) {
        std::string identifier = stream.getString();
        int32_t damage = stream.getVarInt();
        item.mDefinition = context.getItemDefinitions().getDefinition(identifier);
        item.mDamage = damage;
    }

    item.mCount = stream.getLShort();

    int32_t blockRuntimeId = (int32_t) stream.getUnsignedVarInt();
    if (blockRuntimeId != 0) {
        item.mBlockDefinition = context.getBlockDefinitions().getDefinition(context.fromNetworkBlockId(blockRuntimeId));
    }

    uint32_t userDataLength = stream.getUnsignedVarInt();
    std::string userDataBytes = stream.get(userDataLength);
    item.mUserData = userDataBytes;
    if (userDataBytes.empty()) {
        return item;
    }

    ReadOnlyBinaryStream userData(userDataBytes);
    uint16_t nbtSize = userData.getLShort();
    if (nbtSize == 0xffff) {
        unsigned char tagCount = userData.getByte();
        if (tagCount != 1) {
            throw BinaryDataException("Expected 1 tag but got " + std::to_string((int) tagCount));
        }
        item.mTag = NbtIo::readTag(userData, NbtVariant::LittleEndian);
    } else if (nbtSize > 0) {
        item.mTag = NbtIo::readTag(userData, NbtVariant::LittleEndian);
    }

    uint32_t canPlaceLength = readBlockListLength(userData);
    item.mCanPlace.reserve(canPlaceLength);
    for (uint32_t i = 0; i < canPlaceLength; i++) {
        item.mCanPlace.push_back(userData.get(userData.getLShort()));
    }

    uint32_t canBreakLength = readBlockListLength(userData);
    item.mCanBreak.reserve(canBreakLength);
    for (uint32_t i = 0; i < canBreakLength; i++) {
        item.mCanBreak.push_back(userData.get(userData.getLShort()));
    }

    if (item.mDefinition != nullptr && item.mDefinition->getIdentifier() == BLOCKING_ID
            && userData.getRemainingLength() >= 8) {
        item.mBlockingTicks = userData.getLLong();
    }

    return item;
}

ItemStack ItemCodec::readNetworkItemStackDescriptor(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    int runtimeId = (int16_t) stream.getLShort();
    if (runtimeId == 0) {
        stream.getLShort();
        stream.getUnsignedVarInt();
        if (stream.getBool()) {
            stream.getVarInt();
        }
        stream.getUnsignedVarInt();
        stream.get(stream.getUnsignedVarInt());
        return ItemStack::air();
    }

    ItemStack item;
    item.mDefinition = context.getItemDefinitions().getDefinition(context.fromNetworkItemId(runtimeId));
    item.mCount = stream.getLShort();
    item.mDamage = (int) stream.getUnsignedVarInt();

    item.mUsingNetId = stream.getBool();
    if (item.mUsingNetId) {
        item.mNetId = stream.getVarInt();
    }

    int blockRuntimeId = (int) stream.getUnsignedVarInt();
    if (blockRuntimeId != 0) {
        item.mBlockDefinition = context.getBlockDefinitions().getDefinition(context.fromNetworkBlockId(blockRuntimeId));
    }

    uint32_t userDataLength = stream.getUnsignedVarInt();
    if (userDataLength <= 1) {
        item.mDirectUserData = true;
        readDirectUserData(stream, item, userDataLength == 1);
        applyNetworkDamage(item);
        return item;
    }
    std::string userDataBytes = stream.get(userDataLength);
    item.mUserData = userDataBytes;

    ReadOnlyBinaryStream userData(userDataBytes);

    uint16_t nbtSize = userData.getLShort();
    if (nbtSize == 0xffff) {
        unsigned char tagCount = userData.getByte();
        if (tagCount != 1) {
            throw BinaryDataException("Expected 1 tag but got " + std::to_string((int) tagCount));
        }
        item.mTag = NbtIo::readTag(userData, NbtVariant::LittleEndian);
    } else if (nbtSize > 0) {
        item.mTag = NbtIo::readTag(userData, NbtVariant::LittleEndian);
    }

    uint32_t canPlaceLength = readBlockListLength(userData);
    item.mCanPlace.reserve(canPlaceLength);
    for (uint32_t i = 0; i < canPlaceLength; i++) {
        item.mCanPlace.push_back(userData.get(userData.getLShort()));
    }

    uint32_t canBreakLength = readBlockListLength(userData);
    item.mCanBreak.reserve(canBreakLength);
    for (uint32_t i = 0; i < canBreakLength; i++) {
        item.mCanBreak.push_back(userData.get(userData.getLShort()));
    }

    if (item.mDefinition != nullptr && item.mDefinition->getIdentifier() == BLOCKING_ID
            && userData.getRemainingLength() >= 8) {
        item.mBlockingTicks = userData.getLLong();
    }

    applyNetworkDamage(item);
    return item;
}
