#include "Block/DataDrivenBlockDefinitions.h"

#include "BlockDefinitionsNbt.h"
#include "Core/Archive/Gzip.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/NBT/NbtIo.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"

namespace {
    const char *TAG_BLOCKS = "blocks";
    const char *TAG_NAME = "name";
    const char *TAG_PROPERTIES = "properties";

    std::vector<BlockPropertyData> loadDefinitions() {
        std::vector<BlockPropertyData> definitions;

        const std::string compressed((const char *) FalconBlockDefinitionData::kBlockDefinitionsNbt,
                                     FalconBlockDefinitionData::kBlockDefinitionsNbtSize);

        std::string decompressed;
        if (!Gzip::decompress(compressed, decompressed)) {
            LOG_WARN(LogAreaID::Server, "Failed to decompress embedded block definitions");
            return definitions;
        }

        Tag root;
        try {
            ReadOnlyBinaryStream stream(decompressed);
            root = NbtIo::readTag(stream, NbtVariant::BigEndian);
        } catch (const std::exception &exception) {
            LOG_WARN(LogAreaID::Server, "Failed to parse block definitions: %s", exception.what());
            return definitions;
        }

        const Tag *blocks = root.get(TAG_BLOCKS);
        if (blocks == nullptr || !blocks->isList())
            return definitions;

        for (const Tag &block: blocks->getList()) {
            const Tag *name = block.get(TAG_NAME);
            const Tag *properties = block.get(TAG_PROPERTIES);
            if (name == nullptr || properties == nullptr)
                continue;

            BlockPropertyData definition;
            definition.mName = name->asString();
            definition.mProperties = *properties;
            definitions.push_back(definition);
        }

        return definitions;
    }
}

const std::vector<BlockPropertyData> &DataDrivenBlockDefinitions::getAll() {
    static const std::vector<BlockPropertyData> definitions = loadDefinitions();
    return definitions;
}
