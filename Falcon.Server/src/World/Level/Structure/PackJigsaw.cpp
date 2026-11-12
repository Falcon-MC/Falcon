#include "World/Level/Structure/PackJigsaw.h"

#include "Core/Json/Json.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Level.h"
#include "World/Level/Block/BlockNames.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace {
    // the placer's translations of commands.place.generation.error and .outofbounds.error (0x14F432B10)
    const char *const GENERATION_FAILED = "Jigsaw structure generation failed.";
    const char *const OUT_OF_BOUNDS = "Jigsaw structure generation failed. Structure placed out of bounds.";
    std::string readText(const std::filesystem::path &path) {
        std::ifstream file(path, std::ios::binary);
        std::stringstream text;
        text << file.rdbuf();
        return text.str();
    }

    // packs write // and /* */ comments in their json
    std::string stripComments(const std::string &source) {
        std::string out;
        bool inString = false;
        for (size_t index = 0; index < source.size(); ++index) {
            const char c = source[index];
            if (inString) {
                out += c;
                if (c == '\\' && index + 1 < source.size())
                    out += source[++index];
                else if (c == '"')
                    inString = false;
                continue;
            }
            if (c == '"') {
                inString = true;
                out += c;
            } else if (c == '/' && index + 1 < source.size() && source[index + 1] == '/') {
                while (index < source.size() && source[index] != '\n')
                    ++index;
                out += '\n';
            } else if (c == '/' && index + 1 < source.size() && source[index + 1] == '*') {
                index += 2;
                while (index + 1 < source.size() && !(source[index] == '*' && source[index + 1] == '/'))
                    ++index;
                ++index;
            } else {
                out += c;
            }
        }
        return out;
    }

    template <typename Visit>
    void eachJson(const std::filesystem::path &directory, Visit visit) {
        namespace fs = std::filesystem;
        std::error_code error;
        if (!fs::is_directory(directory, error))
            return;
        for (fs::recursive_directory_iterator it(directory, error), end; it != end && !error; it.increment(error)) {
            if (!it->is_regular_file(error) || it->path().extension() != ".json")
                continue;
            const std::unique_ptr<json::Value> root = json::parse(stripComments(readText(it->path())));
            if (root != nullptr && root->isObject())
                visit(*root);
        }
    }

    std::string identifierOf(const json::Value &body) {
        const json::Value *description = body.get("description");
        const json::Value *identifier = description != nullptr ? description->get("identifier") : nullptr;
        return identifier != nullptr ? identifier->string() : std::string();
    }

    BlockState stateOf(const json::Value *value) {
        if (value == nullptr)
            return BlockState(BlockNames::AIR);
        if (value->isString())
            return BlockState(value->string());
        const json::Value *name = value->get("name");
        BlockState state(name != nullptr ? name->string() : "minecraft:air");
        const json::Value *states = value->get("states");
        if (states != nullptr && states->isObject()) {
            for (const std::string &key: states->mKeys) {
                const json::Value &entry = *states->mObject.at(key);
                if (entry.mType == json::Value::Type::Boolean)
                    state.mStates.putByte(key, entry.mBoolean ? 1 : 0);
                else if (entry.isNumber())
                    state.mStates.putInt(key, entry.integer());
                else if (entry.isString())
                    state.mStates.putString(key, entry.string());
            }
        }
        return state;
    }

    // a number, {"type": "constant", "value"} or {"type": "uniform", "min_inclusive", "max_inclusive"}
    bool readIntProvider(const json::Value *value, int32_t &min, int32_t &max) {
        if (value == nullptr)
            return false;
        if (value->isNumber()) {
            min = max = value->integer();
            return true;
        }
        const json::Value *type = value->get("type");
        if (type != nullptr && type->string() == "uniform") {
            const json::Value *low = value->get("min_inclusive");
            const json::Value *high = value->get("max_inclusive");
            if (low == nullptr || high == nullptr || !low->isNumber() || !high->isNumber())
                return false;
            min = low->integer();
            max = std::max(min, high->integer());
            return true;
        }
        const json::Value *constant = value->get("value");
        if (constant == nullptr || !constant->isNumber())
            return false;
        min = max = constant->integer();
        return true;
    }

    // a vertical anchor {"absolute" | "above_bottom" | "below_top": n} (sub_14930A460); from_sea is left out:
    // it adds the dimension's sea level (sub_14383EC10, Dimension +0xCC), whose overworld value is unproven
    bool readAnchor(const json::Value *value, PackJigsaw::Anchor &anchor, int32_t &offset) {
        if (value == nullptr || !value->isObject())
            return false;
        static const std::pair<const char *, PackJigsaw::Anchor> KINDS[] = {
                {"absolute", PackJigsaw::Anchor::Absolute},
                {"above_bottom", PackJigsaw::Anchor::AboveBottom},
                {"below_top", PackJigsaw::Anchor::BelowTop}};
        for (const auto &kind: KINDS) {
            const json::Value *number = value->get(kind.first);
            if (number != nullptr && number->isNumber()) {
                anchor = kind.second;
                offset = number->integer();
                return true;
            }
        }
        return false;
    }

    // start_height: {"type": "constant", "value": anchor} or {"type": "uniform", "min": anchor, "max": anchor}, the
    // height provider schemas (sub_14930A460, sub_1492DB6B0)
    bool readStartHeight(const json::Value *value, PackJigsaw::Structure &structure) {
        if (value == nullptr || !value->isObject())
            return false;
        const json::Value *type = value->get("type");
        if (type == nullptr)
            return false;
        if (type->string() == "uniform") {
            structure.mStartUniform = true;
            return readAnchor(value->get("min"), structure.mStartAnchor, structure.mStartOffset)
                   && readAnchor(value->get("max"), structure.mStartMaxAnchor, structure.mStartMaxOffset);
        }
        return type->string() == "constant"
               && readAnchor(value->get("value"), structure.mStartAnchor, structure.mStartOffset);
    }

    // the game's legacy random (vtable 0x14F1ABF10): 48 bit linear congruence, nextInt as sub_145A2BCC0
    class LegacyRandom {
    public:
        explicit LegacyRandom(int64_t seed) : mState(((uint64_t) seed ^ MULTIPLIER) & MASK) {}

        int32_t next(int bits) {
            mState = (mState * MULTIPLIER + 11) & MASK;
            return (int32_t) (mState >> (48 - bits));
        }

        int64_t nextLong() {
            const int64_t high = next(32);
            const int64_t low = next(32);
            return (int64_t) ((uint64_t) high << 32) + low;
        }

        int32_t nextInt(int32_t bound) {
            if ((bound & -bound) == bound)
                return (int32_t) (((int64_t) bound * (int64_t) next(31)) >> 31);
            int32_t bits;
            int32_t value;
            do {
                bits = next(31);
                value = bits % bound;
            } while ((int32_t) ((uint32_t) bits - (uint32_t) value + (uint32_t) (bound - 1)) < 0);
            return value;
        }

    private:
        static constexpr uint64_t MULTIPLIER = 0x5DEECE66DULL;
        static constexpr uint64_t MASK = (1ULL << 48) - 1ULL;
        uint64_t mState;
    };

    // sub_14383F470: the forked seed mixed with the position's hash
    LegacyRandom positionalRandom(int64_t worldSeed, const Vector3i &at) {
        LegacyRandom base(worldSeed);
        const int64_t fork = base.nextLong();
        int64_t hash = (int64_t) (int32_t) ((uint32_t) at.x * 3129871u) ^ (int64_t) at.z * 116129781LL ^ (int64_t) at.y;
        hash = (int64_t) ((uint64_t) hash * (uint64_t) hash * 42317861ULL + (uint64_t) hash * 11ULL) >> 16;
        return LegacyRandom(hash ^ fork);
    }

    std::vector<std::string> blockNames(const json::Value *value) {
        std::vector<std::string> names;
        if (value == nullptr)
            return names;
        if (!value->isArray()) {
            names.push_back(value->get("name") != nullptr ? value->get("name")->string() : value->string());
            return names;
        }
        for (const std::unique_ptr<json::Value> &entry: value->mArray)
            names.push_back(entry->get("name") != nullptr ? entry->get("name")->string() : entry->string());
        return names;
    }

    PackJigsaw::Rule ruleOf(const json::Value &entry) {
        PackJigsaw::Rule rule;
        if (const json::Value *input = entry.get("input_predicate")) {
            const json::Value *predicate = input->get("predicate_type");
            const json::Value *block = input->get("block");
            const json::Value *probability = input->get("probability");
            rule.mPredicate = predicate != nullptr ? predicate->string() : rule.mPredicate;
            rule.mBlock = block != nullptr ? block->string() : std::string();
            rule.mProbability = probability != nullptr ? (float) probability->number(1.0) : 1.0f;
        }
        if (const json::Value *location = entry.get("location_predicate")) {
            const json::Value *block = location->get("block");
            rule.mLocationBlock = block != nullptr ? block->string() : std::string();
        }
        rule.mOutput = stateOf(entry.get("output_state"));
        return rule;
    }

    // field names from the processor schema (sub_149301D80)
    bool readProcessor(const json::Value &value, PackJigsaw::Processor &processor) {
        const json::Value *type = value.get("processor_type");
        if (type == nullptr)
            return false;
        processor.mType = type->string();
        if (processor.mType == "minecraft:rule") {
            const json::Value *rules = value.get("rules");
            if (rules == nullptr || !rules->isArray())
                return false;
            for (const std::unique_ptr<json::Value> &entry: rules->mArray)
                processor.mRules.push_back(ruleOf(*entry));
            return true;
        }
        if (processor.mType == "minecraft:block_ignore") {
            processor.mBlocks = blockNames(value.get("blocks"));
            return true;
        }
        if (processor.mType == "minecraft:protected_blocks") {
            processor.mBlocks = blockNames(value.get("value"));
            return true;
        }
        if (processor.mType == "minecraft:capped") {
            const json::Value *delegate = value.get("delegate");
            auto inner = std::make_shared<PackJigsaw::Processor>();
            if (delegate == nullptr || !readProcessor(*delegate, *inner)
                || !readIntProvider(value.get("limit"), processor.mLimitMin, processor.mLimitMax))
                return false;
            processor.mDelegate = inner;
            return true;
        }
        return false;
    }

    struct Marker {
        Vector3i mPosition;
        int mFacing = 0;
        std::string mName;
        std::string mTarget;
        std::string mPool;
        BlockState mFinal = BlockState(BlockNames::AIR);
    };

    struct Piece {
        std::shared_ptr<McStructure> mStructure;
        int32_t mRotation = 0;
        Vector3i mOrigin;
        PackJigsaw::Box mBox;
        std::string mProcessors;
        int32_t mDepth = 0;
    };

    // the structure turns clockwise seen from above: north, east, south, west
    int rotateFacing(int facing, int32_t quarterTurns) {
        static const int ORDER[] = {RedstoneFace::NORTH, RedstoneFace::EAST, RedstoneFace::SOUTH, RedstoneFace::WEST};
        for (int index = 0; index < 4; ++index) {
            if (ORDER[index] == facing)
                return ORDER[(index + quarterTurns) & 3];
        }
        return facing;
    }

    PackJigsaw::Box boxOf(const McStructure &structure, int32_t rotation, const Vector3i &origin) {
        const Vector3i size = structure.rotatedSize(rotation);
        return PackJigsaw::Box{origin, Vector3i(origin.x + size.x - 1, origin.y + size.y - 1, origin.z + size.z - 1)};
    }

    bool overlaps(const PackJigsaw::Box &a, const PackJigsaw::Box &b) {
        return a.mMin.x <= b.mMax.x && b.mMin.x <= a.mMax.x && a.mMin.y <= b.mMax.y && b.mMin.y <= a.mMax.y
               && a.mMin.z <= b.mMax.z && b.mMin.z <= a.mMax.z;
    }

    // jigsaw blocks of a piece at its world place
    std::vector<Marker> markersOf(const McStructure &structure, int32_t rotation, const Vector3i &origin) {
        std::vector<Marker> markers;
        McStructure::PlaceOptions options;
        options.mRotation = rotation;
        const Vector3i &size = structure.getSize();
        for (int32_t x = 0; x < size.x; ++x) {
            for (int32_t y = 0; y < size.y; ++y) {
                for (int32_t z = 0; z < size.z; ++z) {
                    const Vector3i local(x, y, z);
                    const BlockState *state = structure.getBlock(local);
                    if (state == nullptr || state->mName != "minecraft:jigsaw")
                        continue;
                    const Vector3i offset = structure.transform(local, options);
                    Marker marker;
                    marker.mPosition = Vector3i(origin.x + offset.x, origin.y + offset.y, origin.z + offset.z);
                    marker.mFacing = rotateFacing(std::clamp(state->mStates.getInt("facing_direction", 0), 0, 5),
                                                  rotation);
                    if (const Tag *data = structure.getBlockEntity(local)) {
                        marker.mName = data->getString("name");
                        marker.mTarget = data->getString("target");
                        marker.mPool = data->getString("target_pool");
                        const std::string final = data->getString("final_state", "minecraft:air");
                        marker.mFinal = BlockState(final.empty() ? "minecraft:air" : final);
                    }
                    markers.push_back(marker);
                }
            }
        }
        return markers;
    }

    // elements by weight, one draw per slot, shuffled
    std::vector<const PackJigsaw::Element *> drawOrder(const PackJigsaw::Pool &pool, std::mt19937 &random) {
        std::vector<const PackJigsaw::Element *> slots;
        for (const PackJigsaw::Element &element: pool.mElements) {
            for (int32_t copy = 0; copy < std::max(element.mWeight, 0); ++copy)
                slots.push_back(&element);
        }
        std::shuffle(slots.begin(), slots.end(), random);
        std::vector<const PackJigsaw::Element *> order;
        for (const PackJigsaw::Element *element: slots) {
            if (std::find(order.begin(), order.end(), element) == order.end())
                order.push_back(element);
        }
        return order;
    }

    std::vector<int32_t> rotations(std::mt19937 &random) {
        std::vector<int32_t> values = {0, 1, 2, 3};
        std::shuffle(values.begin(), values.end(), random);
        return values;
    }
}

// field names from the 26.60 schemas: template pool sub_1492F7D00, jigsaw structure sub_1492DB6B0, processor
// sub_149301D80, rule sub_149301BE0, predicates sub_14930F4D0
void PackJigsaw::addPack(const std::string &packRoot) {
    const std::filesystem::path root = std::filesystem::path(packRoot) / "worldgen";
    eachJson(root / "template_pools", [&](const json::Value &file) {
        const json::Value *body = file.get("minecraft:template_pool");
        if (body == nullptr)
            return;
        const std::string identifier = identifierOf(*body);
        if (identifier.empty())
            return;
        Pool pool;
        const json::Value *fallback = body->get("fallback");
        pool.mFallback = fallback != nullptr ? fallback->string() : std::string();
        const json::Value *elements = body->get("elements");
        if (elements != nullptr && elements->isArray()) {
            for (const std::unique_ptr<json::Value> &entry: elements->mArray) {
                Element element;
                const json::Value *weight = entry->get("weight");
                element.mWeight = weight != nullptr ? weight->integer(1) : 1;
                const json::Value *data = entry->get("element");
                if (data != nullptr) {
                    const json::Value *location = data->get("location");
                    const json::Value *processors = data->get("processors");
                    element.mLocation = location != nullptr ? location->string() : std::string();
                    element.mProcessors = processors != nullptr ? processors->string() : std::string();
                }
                pool.mElements.push_back(element);
            }
        }
        mPools[identifier] = pool;
    });
    eachJson(root / "jigsaw_structures", [&](const json::Value &file) {
        const json::Value *body = file.get("minecraft:jigsaw");
        if (body == nullptr)
            return;
        const std::string identifier = identifierOf(*body);
        if (identifier.empty())
            return;
        Structure structure;
        const json::Value *pool = body->get("start_pool");
        const json::Value *depth = body->get("max_depth");
        structure.mStartPool = pool != nullptr ? pool->string() : std::string();
        structure.mMaxDepth = depth != nullptr ? depth->integer(1) : 1;
        structure.mHasStartHeight = readStartHeight(body->get("start_height"), structure);
        mStructures[identifier] = structure;
    });
    eachJson(root / "processors", [&](const json::Value &file) {
        const json::Value *body = file.get("minecraft:processor_list");
        if (body == nullptr)
            return;
        const std::string identifier = identifierOf(*body);
        if (identifier.empty())
            return;
        std::vector<Processor> list;
        const json::Value *processors = body->get("processors");
        if (processors != nullptr && processors->isArray()) {
            for (const std::unique_ptr<json::Value> &processor: processors->mArray) {
                Processor read;
                if (readProcessor(*processor, read))
                    list.push_back(std::move(read));
            }
        }
        mProcessors[identifier] = std::move(list);
    });
}

// VerticalAnchor resolvers (sub_140EE4B30, sub_14383EBF0, sub_14383EC00, 26.60): below_top counts down from the
// top block
int32_t PackJigsaw::resolveAnchor(Anchor anchor, int32_t offset, int32_t minY, int32_t maxY) {
    switch (anchor) {
        case Anchor::AboveBottom:
            return minY + offset;
        case Anchor::BelowTop:
            return maxY - offset;
        default:
            return offset;
    }
}

int32_t PackJigsaw::startHeight(const Structure &structure, int64_t worldSeed, const Vector3i &location, int32_t minY,
                                int32_t maxY) {
    const int32_t low = resolveAnchor(structure.mStartAnchor, structure.mStartOffset, minY, maxY);
    if (!structure.mStartUniform)
        return low;
    const int32_t high = resolveAnchor(structure.mStartMaxAnchor, structure.mStartMaxOffset, minY, maxY);
    // an empty range only logs and keeps the low end
    if (high < low)
        return low;
    LegacyRandom random = positionalRandom(worldSeed, location);
    return low + random.nextInt(high - low + 1);
}

const PackJigsaw::Pool *PackJigsaw::pool(const std::string &identifier) const {
    const auto found = mPools.find(identifier);
    return found == mPools.end() ? nullptr : &found->second;
}

const PackJigsaw::Structure *PackJigsaw::structure(const std::string &identifier) const {
    const auto found = mStructures.find(identifier);
    return found == mStructures.end() ? nullptr : &found->second;
}

const std::vector<PackJigsaw::Processor> *PackJigsaw::processors(const std::string &identifier) const {
    const auto found = mProcessors.find(identifier);
    return found == mProcessors.end() ? nullptr : &found->second;
}

// a branch grows from each jigsaw until max depth; a piece joins when one of its jigsaws named as the target faces
// back and its box overlaps no placed piece
bool PackJigsaw::place(ServerNetworkHandler &owner, Level &level, const std::string &startPool,
                       const std::string &startJigsaw, int32_t maxDepth, const Vector3i &location,
                       const Options &options, const Finder &finder, int64_t seed, Box &bounds,
                       std::string &error) const {
    std::mt19937 random((uint32_t) seed);
    const Pool *start = pool(startPool);
    if (start == nullptr) {
        error = GENERATION_FAILED;
        return false;
    }

    std::vector<Piece> pieces;
    for (const Element *element: drawOrder(*start, random)) {
        std::shared_ptr<McStructure> structure = element->mLocation.empty() ? nullptr : finder(element->mLocation);
        if (structure == nullptr)
            continue;
        for (const int32_t rotation: rotations(random)) {
            Vector3i origin = location;
            if (!startJigsaw.empty()) {
                bool found = false;
                for (const Marker &marker: markersOf(*structure, rotation, Vector3i(0, 0, 0))) {
                    if (marker.mName != startJigsaw)
                        continue;
                    origin = Vector3i(location.x - marker.mPosition.x, location.y - marker.mPosition.y,
                                      location.z - marker.mPosition.z);
                    found = true;
                    break;
                }
                if (!found)
                    continue;
            }
            pieces.push_back(
                    Piece{structure, rotation, origin, boxOf(*structure, rotation, origin), element->mProcessors, 0});
            break;
        }
        if (!pieces.empty())
            break;
    }
    if (pieces.empty()) {
        error = GENERATION_FAILED;
        return false;
    }

    for (size_t index = 0; index < pieces.size(); ++index) {
        if (pieces[index].mDepth >= maxDepth)
            continue;
        const Piece parent = pieces[index];
        for (const Marker &marker: markersOf(*parent.mStructure, parent.mRotation, parent.mOrigin)) {
            const Pool *target = pool(marker.mPool);
            if (target == nullptr)
                continue;
            std::vector<const Element *> order = drawOrder(*target, random);
            if (const Pool *fallback = pool(target->mFallback)) {
                for (const Element *element: drawOrder(*fallback, random))
                    order.push_back(element);
            }
            const Vector3i joined = RedstoneFace::relative(marker.mPosition, marker.mFacing);
            bool attached = false;
            for (const Element *element: order) {
                if (attached)
                    break;
                std::shared_ptr<McStructure> structure = element->mLocation.empty() ? nullptr
                                                                                    : finder(element->mLocation);
                if (structure == nullptr)
                    continue;
                for (const int32_t rotation: rotations(random)) {
                    for (const Marker &candidate: markersOf(*structure, rotation, Vector3i(0, 0, 0))) {
                        if (candidate.mName != marker.mTarget
                            || candidate.mFacing != RedstoneFace::opposite(marker.mFacing))
                            continue;
                        const Vector3i origin(joined.x - candidate.mPosition.x, joined.y - candidate.mPosition.y,
                                              joined.z - candidate.mPosition.z);
                        const Box box = boxOf(*structure, rotation, origin);
                        bool free = true;
                        for (const Piece &placed: pieces)
                            free = free && !overlaps(placed.mBox, box);
                        if (!free)
                            continue;
                        pieces.push_back(
                                Piece{structure, rotation, origin, box, element->mProcessors, parent.mDepth + 1});
                        attached = true;
                        break;
                    }
                    if (attached)
                        break;
                }
            }
        }
    }

    bounds = pieces.front().mBox;
    for (const Piece &piece: pieces) {
        bounds.mMin = Vector3i(std::min(bounds.mMin.x, piece.mBox.mMin.x), std::min(bounds.mMin.y, piece.mBox.mMin.y),
                               std::min(bounds.mMin.z, piece.mBox.mMin.z));
        bounds.mMax = Vector3i(std::max(bounds.mMax.x, piece.mBox.mMax.x), std::max(bounds.mMax.y, piece.mBox.mMax.y),
                               std::max(bounds.mMax.z, piece.mBox.mMax.z));
    }
    // only a structure without a piece inside the height range fails; the top bound is the exclusive one
    bool inside = false;
    for (const Piece &piece: pieces)
        inside = inside || (piece.mBox.mMin.y >= level.getMinY() && piece.mBox.mMax.y <= level.getMaxY() + 1);
    if (!inside) {
        error = OUT_OF_BOUNDS;
        return false;
    }

    std::uniform_real_distribution<float> chance(0.0f, 1.0f);
    for (const Piece &piece: pieces) {
        std::vector<Marker> markers = markersOf(*piece.mStructure, piece.mRotation, piece.mOrigin);
        const std::vector<Processor> *list = processors(piece.mProcessors);
        // capped: each one changes at most its rolled limit of blocks in the piece
        std::map<const Processor *, int32_t> remaining;
        if (list != nullptr) {
            for (const Processor &processor: *list) {
                if (processor.mType == "minecraft:capped")
                    remaining[&processor] = std::uniform_int_distribution<int32_t>(processor.mLimitMin,
                                                                                   processor.mLimitMax)(random);
            }
        }
        McStructure::PlaceOptions place;
        place.mRotation = piece.mRotation;
        place.mEntities = options.mIncludeEntities;
        place.mKeepWorldWater = options.mApplyWaterlogging;
        place.mFilter = [&](const Vector3i &target, BlockState &state) {
            if (state.mName == "minecraft:jigsaw" && !options.mKeepJigsaws) {
                for (const Marker &marker: markers) {
                    if (marker.mPosition == target) {
                        state = marker.mFinal;
                        return true;
                    }
                }
                state = BlockState(BlockNames::AIR);
                return true;
            }
            if (list == nullptr)
                return true;
            const BlockState world = level.getBlockState(target.x, target.y, target.z);
            const auto applyRules = [&](const Processor &processor) {
                for (const Rule &rule: processor.mRules) {
                    bool input = rule.mPredicate == "minecraft:always_true";
                    if (rule.mPredicate == "minecraft:block_match")
                        input = state.mName == rule.mBlock;
                    else if (rule.mPredicate == "minecraft:random_block_match")
                        input = state.mName == rule.mBlock && chance(random) < rule.mProbability;
                    const bool where = rule.mLocationBlock.empty() || world.mName == rule.mLocationBlock;
                    if (input && where) {
                        state = rule.mOutput;
                        return true;
                    }
                }
                return false;
            };
            const auto listed = [](const std::vector<std::string> &names, const std::string &name) {
                return std::find(names.begin(), names.end(), name) != names.end();
            };
            // -1 leaves the cell out, 1 changed the block, 0 did nothing
            std::function<int(const Processor &)> apply = [&](const Processor &processor) {
                if (processor.mType == "minecraft:block_ignore")
                    return listed(processor.mBlocks, state.mName) ? -1 : 0;
                if (processor.mType == "minecraft:protected_blocks")
                    return listed(processor.mBlocks, world.mName) ? -1 : 0;
                if (processor.mType == "minecraft:rule")
                    return applyRules(processor) ? 1 : 0;
                if (remaining[&processor] <= 0)
                    return 0;
                const int result = apply(*processor.mDelegate);
                if (result != 0)
                    --remaining[&processor];
                return result;
            };
            for (const Processor &processor: *list) {
                if (apply(processor) < 0)
                    return false;
            }
            return true;
        };
        piece.mStructure->place(owner, level, piece.mOrigin, place);
    }
    return true;
}
