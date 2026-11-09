#include "Server/Commands/CommandItems.h"

#include "CommandHiddenItemsJson.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "World/Item/StringToItemParser.h"

#include <memory>
#include <unordered_set>

namespace {
    const std::string MINECRAFT_PREFIX = "minecraft:";

    const std::unordered_set<std::string> &hiddenItems() {
        static const std::unordered_set<std::string> items = []() {
            std::unordered_set<std::string> loaded;
            const std::unique_ptr<json::Value> root = json::parse(FalconCommandData::kCommandHiddenItemsJson);
            const json::Value *list = root == nullptr ? nullptr : root->get("items");
            if (list == nullptr || list->mType != json::Value::Type::Array) {
                LOG_WARN(LogAreaID::Server, "Failed to parse embedded command hidden items");
                return loaded;
            }
            for (const std::unique_ptr<json::Value> &entry: list->mArray)
                loaded.insert(entry->mString);
            return loaded;
        }();
        return items;
    }

    // aliases come without the namespace
    bool isHidden(const std::string &alias) {
        const std::string identifier = alias.rfind(MINECRAFT_PREFIX, 0) == 0 ? alias : MINECRAFT_PREFIX + alias;
        return hiddenItems().count(identifier) != 0;
    }
}

std::vector<std::string> CommandItems::enumValues() {
    std::vector<std::string> values;
    for (const std::string &alias: StringToItemParser::getInstance().getKnownAliases()) {
        if (!isHidden(alias))
            values.push_back(alias);
    }
    return values;
}

bool CommandItems::parse(const std::string &input, Item &out) {
    if (!StringToItemParser::getInstance().parse(input, out))
        return false;
    return !isHidden(out.getIdentifier());
}
