#include "World/Level/Block/BlockPlacementFilter.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/BlockTags.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"

#include <regex>

namespace {
    const std::string MINECRAFT_PREFIX = "minecraft:";

    uint8_t faceBits(const std::string &name) {
        if (name == "down") return 1u << 0;
        if (name == "up") return 1u << 1;
        if (name == "north") return 1u << 2;
        if (name == "south") return 1u << 3;
        if (name == "west") return 1u << 4;
        if (name == "east") return 1u << 5;
        if (name == "side") return 0x3Cu;
        if (name == "all") return 0x3Fu;
        return 0;
    }

    std::string namespaced(const std::string &name) {
        return name.find(':') == std::string::npos ? MINECRAFT_PREFIX + name : name;
    }

    std::string valueText(const json::Value &value) {
        if (value.isString())
            return value.mString;
        if (value.mType == json::Value::Type::Boolean)
            return value.boolean(false) ? "1" : "0";
        return std::to_string((int64_t) value.number(0.0));
    }

    std::string stateText(const Tag &value) {
        if (value.getType() == Tag::Type::String)
            return value.asString();
        if (value.getType() == Tag::Type::Byte)
            return std::to_string(value.asByte());
        return std::to_string(value.asInt());
    }

    // q.any_tag('a', 'b') and q.all_tags(...): the forms block filters use
    bool parseTags(const std::string &expression, BlockPlacementFilter::Match &match) {
        static const std::regex CALL(R"(^\s*(?:q|query)\.(any_tag|all_tags)\s*\(([^)]*)\)\s*$)");
        static const std::regex ARGUMENT(R"('([^']*)')");
        std::smatch call;
        if (!std::regex_match(expression, call, CALL))
            return false;
        match.mAllTags = call[1] == "all_tags";
        const std::string arguments = call[2];
        for (std::sregex_iterator it(arguments.begin(), arguments.end(), ARGUMENT), end; it != end; ++it)
            match.mTags.push_back((*it)[1]);
        return !match.mTags.empty();
    }

    bool matches(const BlockPlacementFilter::Match &match, const BlockState &support) {
        if (!match.mTags.empty()) {
            for (const std::string &tag: match.mTags) {
                const bool has = BlockTags::has(support, tag);
                if (match.mAllTags && !has)
                    return false;
                if (!match.mAllTags && has)
                    return true;
            }
            return match.mAllTags;
        }
        if (support.mName != match.mName)
            return false;
        for (const auto &[name, value]: match.mStates) {
            const Tag *state = support.mStates.get(name);
            if (state == nullptr || stateText(*state) != value)
                return false;
        }
        return true;
    }

    bool accepts(const BlockPlacementFilter::Condition &condition, const BlockState &support) {
        if (condition.mAnyBlock)
            return !DecorationSupport::isAir(support);
        for (const BlockPlacementFilter::Match &match: condition.mBlocks) {
            if (matches(match, support))
                return true;
        }
        return false;
    }
}

BlockPlacementFilter BlockPlacementFilter::parse(const json::Value &component, const std::string &identifier) {
    BlockPlacementFilter filter;
    const json::Value *conditions = component.get("conditions");
    if (conditions == nullptr || !conditions->isArray())
        return filter;
    for (const std::unique_ptr<json::Value> &entry: conditions->mArray) {
        if (!entry->isObject())
            continue;
        Condition condition;
        if (const json::Value *faces = entry->get("allowed_faces"); faces != nullptr && faces->isArray()) {
            condition.mFaces = 0;
            for (const std::unique_ptr<json::Value> &face: faces->mArray)
                condition.mFaces |= faceBits(face->string());
        }
        if (const json::Value *blocks = entry->get("block_filter"); blocks != nullptr && blocks->isArray()) {
            condition.mAnyBlock = false;
            for (const std::unique_ptr<json::Value> &block: blocks->mArray) {
                Match match;
                if (block->isString()) {
                    match.mName = namespaced(block->mString);
                } else if (block->isObject() && block->get("tags") != nullptr) {
                    if (!parseTags(block->get("tags")->string(), match)) {
                        LOG_WARN(LogAreaID::Script, "%s: unsupported placement filter tags \"%s\"", identifier.c_str(),
                                 block->get("tags")->string().c_str());
                        continue;
                    }
                } else if (block->isObject() && block->get("name") != nullptr) {
                    match.mName = namespaced(block->get("name")->string());
                    if (const json::Value *states = block->get("states"); states != nullptr && states->isObject()) {
                        for (const auto &[name, value]: states->mObject)
                            match.mStates.emplace_back(name, valueText(*value));
                    }
                } else {
                    continue;
                }
                condition.mBlocks.push_back(std::move(match));
            }
        }
        filter.mConditions.push_back(std::move(condition));
    }
    return filter;
}

bool BlockPlacementFilter::allowsPlacement(Level &level, const Vector3i &position, int blockFace) const {
    if (isEmpty())
        return true;
    if (blockFace < 0 || blockFace > 5)
        return false;
    const Vector3i at = BlockSupport::supportOf(position, blockFace);
    const BlockState *support = level.peekBlockPtr(at.x, at.y, at.z);
    if (support == nullptr)
        return false;
    for (const Condition &condition: mConditions) {
        if ((condition.mFaces & (1u << blockFace)) != 0 && accepts(condition, *support))
            return true;
    }
    return false;
}

bool BlockPlacementFilter::survives(Level &level, const Vector3i &position) const {
    if (isEmpty())
        return true;
    for (const Condition &condition: mConditions) {
        for (int face = 0; face < 6; face++) {
            if ((condition.mFaces & (1u << face)) == 0)
                continue;
            const Vector3i at = BlockSupport::supportOf(position, face);
            const BlockState *support = level.peekBlockPtr(at.x, at.y, at.z);
            if (support == nullptr || accepts(condition, *support))
                return true;
        }
    }
    return false;
}
