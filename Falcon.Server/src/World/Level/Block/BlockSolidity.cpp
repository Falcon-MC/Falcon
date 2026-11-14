#include "World/Level/Block/BlockSolidity.h"

#include "BlockSolidJson.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"

#include <memory>
#include <unordered_set>

namespace {
    std::unordered_set<std::string> load() {
        std::unordered_set<std::string> solid;
        const std::unique_ptr<json::Value> root = json::parse(FalconBlockSolidData::kBlockSolidJson);
        const json::Value *blocks = root == nullptr ? nullptr : root->get("blocks");
        if (blocks == nullptr || !blocks->isArray()) {
            LOG_WARN(LogAreaID::Server, "Failed to parse embedded block solidity");
            return solid;
        }
        for (const std::unique_ptr<json::Value> &block: blocks->mArray) {
            const json::Value *name = block->get("name");
            const json::Value *value = block->get("solid");
            if (name == nullptr || value == nullptr)
                continue;
            // the table holds one value per type: a per-state value would be lost here
            if (block->get("overrides") != nullptr)
                LOG_WARN(LogAreaID::Server, "Block solidity of %s varies by state", name->mString.c_str());
            if (value->mBoolean)
                solid.insert(name->mString);
        }
        return solid;
    }
}

bool BlockSolidity::isSolid(const std::string &identifier) {
    static const std::unordered_set<std::string> solid = load();
    return solid.count(identifier) != 0;
}
