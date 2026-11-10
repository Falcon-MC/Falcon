#include "Server/Commands/CommandEnums.h"

#include "CommandHiddenBlocksJson.h"
#include "CommandHiddenItemsJson.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Server/Commands/SetBlockCommand.h"
#include "World/Item/StringToItemParser.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Block/BlockState.h"

#include <memory>
#include <unordered_set>

namespace {
    const std::string MINECRAFT_PREFIX = "minecraft:";

    std::unordered_set<std::string> loadNames(const char *text, const char *field, const char *what) {
        std::unordered_set<std::string> names;
        const std::unique_ptr<json::Value> root = json::parse(text);
        const json::Value *list = root == nullptr ? nullptr : root->get(field);
        if (list == nullptr || list->mType != json::Value::Type::Array) {
            LOG_WARN(LogAreaID::Server, "Failed to parse embedded command hidden %s", what);
            return names;
        }
        for (const std::unique_ptr<json::Value> &entry: list->mArray)
            names.insert(entry->mString);
        return names;
    }

    const std::unordered_set<std::string> &hiddenItems() {
        static const std::unordered_set<std::string> items = loadNames(FalconCommandData::kCommandHiddenItemsJson,
                                                                       "items", "items");
        return items;
    }

    const std::unordered_set<std::string> &hiddenBlocks() {
        static const std::unordered_set<std::string> blocks = loadNames(FalconCommandData::kCommandHiddenBlocksJson,
                                                                        "blocks", "blocks");
        return blocks;
    }

    // enum values come without the namespace
    std::string namespaced(const std::string &name) {
        return name.rfind(MINECRAFT_PREFIX, 0) == 0 ? name : MINECRAFT_PREFIX + name;
    }

    std::string bare(const std::string &name) {
        return name.rfind(MINECRAFT_PREFIX, 0) == 0 ? name.substr(MINECRAFT_PREFIX.size()) : name;
    }
}

std::vector<std::string> CommandEnums::itemValues() {
    std::vector<std::string> values;
    for (const std::string &alias: StringToItemParser::getInstance().getKnownAliases()) {
        if (hiddenItems().count(namespaced(alias)) == 0)
            values.push_back(alias);
    }
    return values;
}

bool CommandEnums::parseItem(const std::string &input, Item &out) {
    if (!StringToItemParser::getInstance().parse(input, out))
        return false;
    return hiddenItems().count(namespaced(out.getIdentifier())) == 0;
}

std::vector<std::string> CommandEnums::blockValues() {
    BlockPaletteRegistry &registry = BlockPaletteRegistry::getInstance();
    registry.initialize();
    std::vector<std::string> values;
    for (const std::string &name: registry.getBlockNames()) {
        if (hiddenBlocks().count(namespaced(name)) == 0)
            values.push_back(bare(name));
    }
    // blocks of packs, scripts and plugins are named in commands too
    for (const CustomBlockDefinition &block: CustomContentRegistry::getInstance().getBlocks())
        values.push_back(block.mIdentifier);
    return values;
}

bool CommandEnums::parseBlock(const std::string &input, BlockState &out) {
    if (hiddenBlocks().count(namespaced(input)) != 0)
        return false;
    if (SetBlockCommand::resolveBlock(input, out))
        return true;
    // a custom block has no permutations of its own
    if (CustomContentRegistry::getInstance().findBlock(input) == nullptr)
        return false;
    out = BlockState(input, Tag::ofCompound());
    return true;
}
