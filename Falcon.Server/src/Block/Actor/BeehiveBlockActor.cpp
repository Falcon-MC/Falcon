#include "Block/Actor/BeehiveBlockActor.h"

#include "Actor/AI/Control/LookControl.h"
#include "Actor/Mob/MobActor.h"
#include "Block/BlockShape.h"
#include "Block/Blocks/BeehiveBlock.h"
#include "Block/Blocks/FireBlock.h"
#include "Block/Blocks/LiquidView.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Components/PlacementOrientation.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cstddef>
#include <random>
#include <utility>

namespace {
    const char *const TAG_OCCUPANTS = "Occupants";
    const char *const TAG_SHOULD_SPAWN_BEES = "ShouldSpawnBees";
    const char *const TAG_ACTOR_IDENTIFIER = "ActorIdentifier";
    const char *const TAG_SAVE_DATA = "SaveData";
    const char *const TAG_TICKS_LEFT_TO_STAY = "TicksLeftToStay";
    const char *const TAG_HEALTH = "Health";
    const char *const NECTAR_PROPERTY = "minecraft:has_nectar";
    const char *const EXITED_HIVE_EVENT = "minecraft:exited_hive";
    const char *const EXITED_DISTURBED_HIVE_EVENT = "minecraft:exited_disturbed_hive";
    const char *const EXITED_HIVE_ON_FIRE_EVENT = "minecraft:exited_hive_on_fire";
    const char *const ENTER_SOUND = "block.beehive.enter";
    const char *const EXIT_SOUND = "block.beehive.exit";
    const char *const WORK_SOUND = "block.beehive.work";
    const int32_t NECTAR_STAY_TICKS = 2400;
    const int32_t STAY_TICKS = 600;
    const int32_t BLOCKED_EXIT_RETRY_TICKS = 600;
    const float WORK_SOUND_CHANCE = 0.005f;
    const float EXIT_DISTANCE = 0.55f;
    const int HORIZONTAL_FACES[] = {PlacementOrientation::FACE_NORTH, PlacementOrientation::FACE_SOUTH,
                                    PlacementOrientation::FACE_WEST, PlacementOrientation::FACE_EAST};
    const int VERTICAL_FACES[] = {PlacementOrientation::FACE_UP, PlacementOrientation::FACE_DOWN};

    std::mt19937 &hiveRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    std::string storedIdentifier(const std::string &actorIdentifier) {
        const size_t suffix = actorIdentifier.find('<');
        return suffix == std::string::npos ? actorIdentifier : actorIdentifier.substr(0, suffix);
    }

    Vector3f blockCenter(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }

    bool isOpen(Level &level, const Vector3i &position) {
        const BlockState *state = level.peekBlockPtr(position.x, position.y, position.z);
        return state != nullptr && !BlockShape::hasCollision(*state) && !LiquidView(*state).isLiquid();
    }

    Vector3f exitPosition(const Vector3i &hive, int face, const ActorSize &size) {
        const Vector3f center = blockCenter(hive);
        if (face < 0)
            return Vector3f(center.x, center.y - size.mHeight * 0.5f, center.z);

        const Vector3i offset = PlacementOrientation::faceOffset(face);
        const float horizontal = EXIT_DISTANCE + size.mWidth * 0.5f;
        const float vertical = 0.5f + size.mHeight * 0.5f;
        return Vector3f(center.x + (float) offset.x * horizontal,
                        center.y - size.mHeight * 0.5f + (float) offset.y * vertical,
                        center.z + (float) offset.z * horizontal);
    }
}

Tag BeehiveBlockActor::saveNbt() const {
    Tag occupants = Tag::ofList(Tag::Type::Compound);
    for (const Occupant &occupant: mOccupants) {
        Tag entry = Tag::ofCompound();
        entry.putString(TAG_ACTOR_IDENTIFIER, occupant.mActorIdentifier);
        entry.put(TAG_SAVE_DATA, occupant.mSaveData);
        entry.putInt(TAG_TICKS_LEFT_TO_STAY, occupant.mTicksLeftToStay);
        occupants.addToList(std::move(entry));
    }

    Tag data = Tag::ofCompound();
    data.put(TAG_OCCUPANTS, std::move(occupants));
    data.putByte(TAG_SHOULD_SPAWN_BEES, 0);
    return data;
}

void BeehiveBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) context;

    mOccupants.clear();
    const Tag *occupants = data.get(TAG_OCCUPANTS);
    if (occupants == nullptr || !occupants->isList())
        return;

    for (const Tag &entry: occupants->getList()) {
        if (!entry.isCompound())
            continue;

        const Tag *saveData = entry.get(TAG_SAVE_DATA);
        addOccupant(entry.getString(TAG_ACTOR_IDENTIFIER),
                    saveData != nullptr && saveData->isCompound() ? *saveData : Tag::ofCompound(),
                    entry.getInt(TAG_TICKS_LEFT_TO_STAY));
    }
}

void BeehiveBlockActor::tickAll(ServerNetworkHandler &owner) {
    for (Level *level: owner.getLevels()) {
        for (BeehiveBlockActor *hive: level->getBlockActors().findAll<BeehiveBlockActor>()) {
            const Vector3i position = hive->getPosition();
            if (!hive->tick(owner))
                level->getBlockActors().remove(position);
        }
    }
}

bool BeehiveBlockActor::isFull() const {
    return (int32_t) mOccupants.size() >= MAX_OCCUPANTS;
}

bool BeehiveBlockActor::isEmpty() const {
    return mOccupants.empty();
}

void BeehiveBlockActor::addOccupant(const std::string &actorIdentifier, Tag saveData, int32_t ticksLeftToStay) {
    Occupant occupant;
    occupant.mActorIdentifier = actorIdentifier;
    occupant.mSaveData = std::move(saveData);
    occupant.mTicksLeftToStay = ticksLeftToStay;
    mOccupants.push_back(std::move(occupant));
}

void BeehiveBlockActor::clearOccupants() {
    mOccupants.clear();
}

bool BeehiveBlockActor::admit(ServerNetworkHandler &owner, MobActor &mob) {
    if (mLevel == nullptr || isFull() || mob.isExpired() || !mob.isAlive() || _isFireNearby(*mLevel))
        return false;

    const bool hasNectar = mob.getIntProperty(NECTAR_PROPERTY, 0) != 0;
    addOccupant(mob.getIdentifier(), mob.saveNbt(), hasNectar ? NECTAR_STAY_TICKS : STAY_TICKS);
    mob.despawn();
    owner.playLevelSound(*mLevel, ENTER_SOUND, blockCenter(mPosition));
    return true;
}

void BeehiveBlockActor::evacuate(ServerNetworkHandler &owner, bool hiveRemains) {
    if (mLevel == nullptr || mOccupants.empty())
        return;

    Level &level = *mLevel;
    const std::string event = _isFireNearby(level) ? EXITED_HIVE_ON_FIRE_EVENT : EXITED_DISTURBED_HIVE_EVENT;
    const std::vector<int> faces = hiveRemains ? _spawnFaces(level, -1, true) : std::vector<int>();
    if (hiveRemains && faces.empty())
        return;

    std::vector<Occupant> occupants;
    occupants.swap(mOccupants);
    for (const Occupant &occupant: occupants) {
        const int face = faces.empty()
                         ? -1
                         : faces[std::uniform_int_distribution<size_t>(0, faces.size() - 1)(hiveRandom())];
        _release(owner, level, occupant, face, event, false);
    }
}

bool BeehiveBlockActor::tick(ServerNetworkHandler &owner) {
    if (mLevel == nullptr)
        return true;

    Level &level = *mLevel;
    const BlockState *state = level.peekBlockPtr(mPosition.x, mPosition.y, mPosition.z);
    if (state == nullptr)
        return true;

    if (VanillaBlocks::getAs<BeehiveBlock>(state->mName) == nullptr) {
        evacuate(owner, false);
        return false;
    }

    if (mOccupants.empty())
        return true;

    if (_isFireNearby(level)) {
        evacuate(owner, true);
        return true;
    }

    const int frontFace = BeehiveBlock::getFrontFace(*state);
    const bool sheltered = level.isRaining() || level.isNight();
    std::vector<int> faces;
    bool facesResolved = false;

    for (size_t index = 0; index < mOccupants.size();) {
        Occupant &occupant = mOccupants[index];
        if (occupant.mTicksLeftToStay > 0)
            --occupant.mTicksLeftToStay;

        if (occupant.mTicksLeftToStay > 0 || sheltered) {
            if (std::uniform_real_distribution<float>(0.0f, 1.0f)(hiveRandom()) < WORK_SOUND_CHANCE)
                owner.playLevelSound(level, WORK_SOUND, blockCenter(mPosition));
            ++index;
            continue;
        }

        if (!facesResolved) {
            faces = _spawnFaces(level, frontFace, false);
            facesResolved = true;
        }

        if (faces.empty()) {
            occupant.mTicksLeftToStay = BLOCKED_EXIT_RETRY_TICKS;
            ++index;
            continue;
        }

        const Occupant released = std::move(occupant);
        mOccupants.erase(mOccupants.begin() + (std::ptrdiff_t) index);
        const int face = faces[std::uniform_int_distribution<size_t>(0, faces.size() - 1)(hiveRandom())];
        _release(owner, level, released, face, EXITED_HIVE_EVENT, true);
    }

    return true;
}

std::vector<int> BeehiveBlockActor::_spawnFaces(Level &level, int frontFace, bool vertical) const {
    if (frontFace >= 0 && isOpen(level, PlacementOrientation::relativePosition(mPosition, frontFace)))
        return {frontFace};

    std::vector<int> faces;
    for (const int face: HORIZONTAL_FACES) {
        if (isOpen(level, PlacementOrientation::relativePosition(mPosition, face)))
            faces.push_back(face);
    }

    if (!vertical)
        return faces;

    for (const int face: VERTICAL_FACES) {
        if (isOpen(level, PlacementOrientation::relativePosition(mPosition, face)))
            faces.push_back(face);
    }
    return faces;
}

bool BeehiveBlockActor::_isFireNearby(Level &level) const {
    for (int32_t x = mPosition.x - 1; x <= mPosition.x + 1; ++x) {
        for (int32_t y = mPosition.y - 1; y <= mPosition.y + 1; ++y) {
            for (int32_t z = mPosition.z - 1; z <= mPosition.z + 1; ++z) {
                const BlockState *state = level.peekBlockPtr(x, y, z);
                if (state != nullptr && VanillaBlocks::getAs<FireBlock>(state->mName) != nullptr)
                    return true;
            }
        }
    }
    return false;
}

void BeehiveBlockActor::_release(ServerNetworkHandler &owner, Level &level, const Occupant &occupant, int face,
                                 const std::string &event, bool deliverNectar) {
    const std::string identifier = storedIdentifier(occupant.mActorIdentifier);
    if (identifier.empty())
        return;

    const Vector3i hive = mPosition;
    ServerActor *actor = owner.spawnActor(level, identifier, blockCenter(hive),
                                          [&owner, &occupant, &hive, face](ServerActor &spawned) {
        spawned.loadNbt(occupant.mSaveData);
        if (MobActor *mob = dynamic_cast<MobActor *>(&spawned))
            mob->getEquipment().loadNbt(occupant.mSaveData, owner.getCodecContext());

        const Vector3f position = exitPosition(hive, face, spawned.getSize());
        spawned.setPosition(position);
        spawned.setMotion(Vector3f(0.0f, 0.0f, 0.0f));
        if (face < 0)
            return;

        const Vector3i offset = PlacementOrientation::faceOffset(face);
        const Vector3f ahead(position.x + (float) offset.x, position.y, position.z + (float) offset.z);
        spawned.setRotation(Vector3f(0.0f, LookControl::yawTowards(position, ahead), 0.0f));
    });

    owner.playLevelSound(level, EXIT_SOUND, blockCenter(hive));

    MobActor *mob = dynamic_cast<MobActor *>(actor);
    if (mob == nullptr)
        return;

    mob->setHealth(occupant.mSaveData.getFloat(TAG_HEALTH, mob->getHealth()));

    if (deliverNectar && mob->getIntProperty(NECTAR_PROPERTY, 0) != 0) {
        const BlockState *state = level.peekBlockPtr(hive.x, hive.y, hive.z);
        const int32_t honeyLevel = state == nullptr ? 0 : BeehiveBlock::getHoneyLevel(*state);
        if (state != nullptr && honeyLevel < BeehiveBlock::MAX_HONEY_LEVEL)
            BeehiveBlock::setHoneyLevel(level, hive, honeyLevel + 1);
    }

    if (face >= 0)
        mob->setHomePosition(blockCenter(hive));

    mob->fireEvent(owner, event);
}
