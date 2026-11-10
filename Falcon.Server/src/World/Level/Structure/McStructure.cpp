#include "World/Level/Structure/McStructure.h"

#include "Core/NBT/NbtIo.h"
#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Block/Actor/BlockActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace {
    Tag intList(const Vector3i &value) {
        return Tag::ofList(Tag::Type::Int, {Tag::ofInt(value.x), Tag::ofInt(value.y), Tag::ofInt(value.z)});
    }

    bool readIntList(const Tag *list, Vector3i &out) {
        if (list == nullptr || !list->isList() || list->getList().size() != 3)
            return false;
        const std::vector<Tag> &values = list->getList();
        for (const Tag &value: values) {
            if (value.getType() != Tag::Type::Int)
                return false;
        }
        out = Vector3i(values[0].asInt(), values[1].asInt(), values[2].asInt());
        return true;
    }

    bool sameState(const BlockState &left, const BlockState &right) {
        return left.mName == right.mName && left.mStates == right.mStates;
    }
}

McStructure::McStructure(const Vector3i &size) : mSize(size) {
    const size_t volume = (size_t) std::max(size.x, 0) * (size_t) std::max(size.y, 0) * (size_t) std::max(size.z, 0);
    for (std::vector<int32_t> &layer: mLayers)
        layer.assign(volume, VOID_INDEX);
}

bool McStructure::contains(const Vector3i &local) const {
    return local.x >= 0 && local.y >= 0 && local.z >= 0 && local.x < mSize.x && local.y < mSize.y && local.z < mSize.z;
}

size_t McStructure::_index(const Vector3i &local) const {
    return ((size_t) local.x * (size_t) mSize.y + (size_t) local.y) * (size_t) mSize.z + (size_t) local.z;
}

int32_t McStructure::_paletteIndex(const BlockState &state) {
    for (size_t index = 0; index < mPalette.size(); ++index) {
        if (sameState(mPalette[index], state))
            return (int32_t) index;
    }
    mPalette.push_back(BlockState(state.mName, state.mStates));
    return (int32_t) mPalette.size() - 1;
}

const BlockState *McStructure::getBlock(const Vector3i &local, int32_t layer) const {
    if (!contains(local) || layer < 0 || layer > 1)
        return nullptr;
    const int32_t index = mLayers[layer][_index(local)];
    return index < 0 || (size_t) index >= mPalette.size() ? nullptr : &mPalette[(size_t) index];
}

void McStructure::setBlock(const Vector3i &local, const BlockState *state, int32_t layer) {
    if (!contains(local) || layer < 0 || layer > 1)
        return;
    mLayers[layer][_index(local)] = state == nullptr ? VOID_INDEX : _paletteIndex(*state);
}

bool McStructure::isWaterlogged(const Vector3i &local) const {
    const BlockState *overlay = getBlock(local, 1);
    return overlay != nullptr && (overlay->mName == "minecraft:water" || overlay->mName == "minecraft:flowing_water");
}

const Tag *McStructure::getBlockEntity(const Vector3i &local) const {
    if (!contains(local))
        return nullptr;
    const auto found = mBlockEntities.find((int32_t) _index(local));
    return found == mBlockEntities.end() ? nullptr : &found->second;
}

void McStructure::setBlockEntity(const Vector3i &local, Tag data) {
    if (contains(local))
        mBlockEntities[(int32_t) _index(local)] = std::move(data);
}

Tag McStructure::toNbt() const {
    Tag indices = Tag::ofList(Tag::Type::List);
    for (const std::vector<int32_t> &layer: mLayers) {
        std::vector<Tag> values;
        values.reserve(layer.size());
        for (const int32_t index: layer)
            values.push_back(Tag::ofInt(index));
        indices.addToList(Tag::ofList(Tag::Type::Int, std::move(values)));
    }

    Tag palette = Tag::ofList(Tag::Type::Compound);
    for (const BlockState &state: mPalette)
        palette.addToList(state.toNbt());

    Tag positions = Tag::ofCompound();
    for (const auto &entry: mBlockEntities) {
        Tag data = Tag::ofCompound();
        data.put("block_entity_data", entry.second);
        positions.put(std::to_string(entry.first), std::move(data));
    }

    Tag defaults = Tag::ofCompound();
    defaults.put("block_palette", std::move(palette));
    defaults.put("block_position_data", std::move(positions));
    Tag palettes = Tag::ofCompound();
    palettes.put("default", std::move(defaults));

    Tag structure = Tag::ofCompound();
    structure.put("block_indices", std::move(indices));
    structure.put("entities", Tag::ofList(Tag::Type::Compound, mEntities));
    structure.put("palette", std::move(palettes));

    Tag root = Tag::ofCompound();
    root.putInt("format_version", FORMAT_VERSION);
    root.put("size", intList(mSize));
    root.put("structure", std::move(structure));
    root.put("structure_world_origin", intList(mWorldOrigin));
    return root;
}

bool McStructure::fromNbt(const Tag &root, McStructure &out) {
    if (!root.isCompound())
        return false;
    Vector3i size;
    const Tag *structure = root.get("structure");
    if (!readIntList(root.get("size"), size) || size.x < 0 || size.y < 0 || size.z < 0 || structure == nullptr
        || !structure->isCompound())
        return false;

    McStructure parsed(size);
    readIntList(root.get("structure_world_origin"), parsed.mWorldOrigin);
    const size_t volume = parsed.mLayers[0].size();

    const Tag *palettes = structure->get("palette");
    const Tag *defaults = palettes != nullptr && palettes->isCompound() ? palettes->get("default") : nullptr;
    const Tag *palette = defaults != nullptr && defaults->isCompound() ? defaults->get("block_palette") : nullptr;
    if (palette != nullptr && palette->isList()) {
        for (const Tag &entry: palette->getList()) {
            const Tag *states = entry.isCompound() ? entry.get("states") : nullptr;
            parsed.mPalette.emplace_back(entry.isCompound() ? entry.getString("name", "minecraft:air")
                                                            : "minecraft:air",
                                         states != nullptr && states->isCompound() ? *states : Tag::ofCompound());
        }
    }

    const Tag *indices = structure->get("block_indices");
    if (indices != nullptr && indices->isList()) {
        const std::vector<Tag> &layers = indices->getList();
        for (size_t layer = 0; layer < 2 && layer < layers.size(); ++layer) {
            if (!layers[layer].isList())
                return false;
            const std::vector<Tag> &values = layers[layer].getList();
            // the game writes an empty list for a layer it has nothing in
            if (values.empty())
                continue;
            if (values.size() != volume)
                return false;
            for (size_t cell = 0; cell < volume; ++cell) {
                const int32_t index = values[cell].getType() == Tag::Type::Int ? values[cell].asInt() : VOID_INDEX;
                parsed.mLayers[layer][cell] = index < 0 || (size_t) index >= parsed.mPalette.size() ? VOID_INDEX
                                                                                                    : index;
            }
        }
    }

    const Tag *positions = defaults != nullptr && defaults->isCompound() ? defaults->get("block_position_data")
                                                                         : nullptr;
    if (positions != nullptr && positions->isCompound()) {
        const std::vector<std::string> &keys = positions->getKeys();
        const std::vector<Tag> &values = positions->getValues();
        for (size_t index = 0; index < keys.size(); ++index) {
            const Tag *data = values[index].isCompound() ? values[index].get("block_entity_data") : nullptr;
            char *end = nullptr;
            const long cell = std::strtol(keys[index].c_str(), &end, 10);
            if (data != nullptr && data->isCompound() && end != keys[index].c_str() && cell >= 0
                && (size_t) cell < volume)
                parsed.mBlockEntities[(int32_t) cell] = *data;
        }
    }

    const Tag *entities = structure->get("entities");
    if (entities != nullptr && entities->isList()) {
        for (const Tag &entity: entities->getList()) {
            if (entity.isCompound())
                parsed.mEntities.push_back(entity);
        }
    }

    out = std::move(parsed);
    return true;
}

std::string McStructure::serialize() const {
    BinaryStream stream;
    NbtIo::writeTag(stream, toNbt(), NbtVariant::LittleEndian);
    return stream.getBuffer();
}

bool McStructure::parse(const std::string &bytes, McStructure &out) {
    try {
        ReadOnlyBinaryStream stream(bytes);
        return fromNbt(NbtIo::readTag(stream, NbtVariant::LittleEndian), out);
    } catch (const std::exception &) {
        return false;
    }
}

McStructure McStructure::capture(ServerNetworkHandler &owner, Level &level, const Vector3i &from, const Vector3i &to,
                                 bool blocks, bool entities) {
    const Vector3i low(std::min(from.x, to.x), std::min(from.y, to.y), std::min(from.z, to.z));
    const Vector3i high(std::max(from.x, to.x), std::max(from.y, to.y), std::max(from.z, to.z));
    McStructure structure(Vector3i(high.x - low.x + 1, high.y - low.y + 1, high.z - low.z + 1));
    structure.mWorldOrigin = low;

    if (blocks) {
        for (int32_t x = low.x; x <= high.x; ++x) {
            for (int32_t y = low.y; y <= high.y; ++y) {
                for (int32_t z = low.z; z <= high.z; ++z) {
                    const Vector3i local(x - low.x, y - low.y, z - low.z);
                    if (y < level.getMinY() || y > level.getMaxY()) {
                        const BlockState air;
                        structure.setBlock(local, &air);
                        continue;
                    }
                    const BlockState state = level.getBlockState(x, y, z);
                    structure.setBlock(local, &state);
                    const BlockState overlay = level.getBlockStateAtLayer(x, y, z, 1);
                    if (overlay.mName != "minecraft:air")
                        structure.setBlock(local, &overlay, 1);
                    if (BlockActor *actor = level.getBlockActors().find(Vector3i(x, y, z)))
                        structure.setBlockEntity(local, actor->saveNbt());
                }
            }
        }
    }

    if (entities) {
        for (auto &entry: owner.getActors()) {
            ServerActor &actor = *entry.second;
            const Vector3f &position = actor.getPosition();
            if (!actor.isIn(level) || !actor.isAlive() || position.x < (float) low.x
                || position.x >= (float) high.x + 1.0f || position.y < (float) low.y
                || position.y >= (float) high.y + 1.0f || position.z < (float) low.z
                || position.z >= (float) high.z + 1.0f)
                continue;
            structure.addEntity(actor.saveNbt());
        }
    }
    return structure;
}

Vector3i McStructure::rotatedSize(int32_t rotation) const {
    return (rotation & 1) != 0 ? Vector3i(mSize.z, mSize.y, mSize.x) : mSize;
}

// TODO: mirror then rotate, as the structure block shows it; the game's order and its state rotation unverified
Vector3i McStructure::transform(const Vector3i &local, const PlaceOptions &options) const {
    int32_t x = local.x, z = local.z;
    if (options.mMirror == Mirror::X || options.mMirror == Mirror::XZ)
        x = mSize.x - 1 - x;
    if (options.mMirror == Mirror::Z || options.mMirror == Mirror::XZ)
        z = mSize.z - 1 - z;
    switch (options.mRotation & 3) {
        case 1:
            return Vector3i(mSize.z - 1 - z, local.y, x);
        case 2:
            return Vector3i(mSize.x - 1 - x, local.y, mSize.z - 1 - z);
        case 3:
            return Vector3i(z, local.y, mSize.x - 1 - x);
        default:
            return Vector3i(x, local.y, z);
    }
}

void McStructure::place(ServerNetworkHandler &owner, Level &level, const Vector3i &at,
                        const PlaceOptions &options) const {
    // TODO: integrity draws from the game's random, std::mt19937 stands in
    std::mt19937 random((uint32_t) options.mSeed);
    std::uniform_real_distribution<float> chance(0.0f, 1.0f);

    if (options.mBlocks) {
        for (int32_t x = 0; x < mSize.x; ++x) {
            for (int32_t y = 0; y < mSize.y; ++y) {
                for (int32_t z = 0; z < mSize.z; ++z) {
                    const Vector3i local(x, y, z);
                    const BlockState *state = getBlock(local);
                    if (state == nullptr)
                        continue;
                    if (options.mIntegrity < 1.0f && chance(random) > options.mIntegrity)
                        continue;
                    const Vector3i offset = transform(local, options);
                    const Vector3i target(at.x + offset.x, at.y + offset.y, at.z + offset.z);
                    if (target.y < level.getMinY() || target.y > level.getMaxY()
                        || level.peekBlockPtr(target.x, target.y, target.z) == nullptr)
                        continue;

                    BlockState chosen = *state;
                    if (options.mFilter && !options.mFilter(target, chosen))
                        continue;
                    state = &chosen;
                    level.getBlockActors().remove(target);
                    level.setBlock(target, *state, true);
                    BlockActionHandler::broadcastBlockUpdate(owner, level, target, *state);

                    const BlockState *overlay = getBlock(local, 1);
                    BlockState water("minecraft:water");
                    water.mStates.putInt("liquid_depth", 0);
                    const BlockState *layer1 = overlay != nullptr ? overlay : (options.mWaterlogged ? &water : nullptr);
                    if (layer1 != nullptr) {
                        level.setBlockStateAtLayer(target.x, target.y, target.z, 1, *layer1);
                        BlockActionHandler::broadcastBlockUpdate(owner, level, target, *layer1, 1);
                    }

                    const Tag *data = getBlockEntity(local);
                    std::unique_ptr<BlockActor> actor = data != nullptr ? BlockActorStore::create(data->getString("id"))
                                                                        : nullptr;
                    if (actor == nullptr)
                        continue;
                    Tag moved = *data;
                    moved.putInt("x", target.x);
                    moved.putInt("y", target.y);
                    moved.putInt("z", target.z);
                    actor->loadNbt(moved, owner.getCodecContext());
                    actor->setPosition(target);
                    BlockActor &placed = *actor;
                    level.getBlockActors().insert(std::move(actor));
                    BlockActionHandler::broadcastBlockActorData(owner, level, placed);
                }
            }
        }
    }

    if (!options.mEntities)
        return;
    for (const Tag &entity: mEntities) {
        const std::string identifier = entity.getString("identifier");
        const Tag *pos = entity.get("Pos");
        if (identifier.empty() || pos == nullptr || !pos->isList() || pos->getList().size() != 3)
            continue;
        const std::vector<Tag> &values = pos->getList();
        // the saved position is the world one: offset from where the structure was taken
        const float lx = values[0].asFloat() - (float) mWorldOrigin.x;
        const float ly = values[1].asFloat() - (float) mWorldOrigin.y;
        const float lz = values[2].asFloat() - (float) mWorldOrigin.z;
        const int32_t cellX = (int32_t) std::floor(lx), cellZ = (int32_t) std::floor(lz);
        const Vector3i cell = transform(Vector3i(cellX, 0, cellZ), options);
        const Vector3f target((float) at.x + (float) cell.x + 0.5f, (float) at.y + ly,
                              (float) at.z + (float) cell.z + 0.5f);
        Tag data = entity;
        data.remove("UniqueID");
        data.remove("Pos");
        owner.spawnActor(level, identifier, target, [&](ServerActor &actor) {
            const int64_t uniqueId = actor.getUniqueId();
            actor.loadNbt(data);
            actor.setUniqueId(uniqueId);
            actor.setPosition(target);
        });
    }
}
