#include "World/Level/Block/Actor/JukeboxBlockActor.h"

#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Inventory/ItemStackNbt.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Level.h"

#include <cstring>

namespace {
    const char *const TAG_RECORD_ITEM = "RecordItem";
    const char *const TAG_FINISHED_RECORDING = "FinishedRecording";
    const char *const TAG_TICKS_PLAYING = "TicksPlaying";

    const char *const RECORD_PREFIX = "minecraft:music_disc_";

    // the record item's constructor in the game: comparator signal and length in seconds, per sound event
    const JukeboxBlockActor::Song SONGS[] = {
            {"13", 1, 178.0f},       {"cat", 2, 185.0f},     {"blocks", 3, 345.0f},  {"chirp", 4, 185.0f},
            {"far", 5, 174.0f},      {"mall", 6, 197.0f},    {"mellohi", 7, 96.0f},  {"stal", 8, 150.0f},
            {"strad", 9, 188.0f},    {"ward", 10, 251.0f},   {"11", 11, 71.0f},      {"wait", 12, 238.0f},
            {"pigstep", 13, 149.0f}, {"otherside", 14, 195.0f}, {"5", 15, 178.0f},   {"relic", 14, 218.0f},
            {"creator", 12, 176.0f}, {"creator_music_box", 11, 73.0f}, {"precipice", 13, 299.0f},
            {"tears", 10, 175.0f},   {"lava_chicken", 9, 134.0f}, {"bounce", 8, 234.0f}};

    constexpr int32_t TICKS_PER_SECOND = 20;
}

const JukeboxBlockActor::Song *JukeboxBlockActor::findSong(const ItemStack &record) {
    if (record.isAir())
        return nullptr;
    const std::string &identifier = record.mDefinition->getIdentifier();
    const size_t prefix = std::strlen(RECORD_PREFIX);
    if (identifier.compare(0, prefix, RECORD_PREFIX) != 0)
        return nullptr;
    for (const Song &song: SONGS) {
        if (identifier.compare(prefix, std::string::npos, song.mDisc) == 0)
            return &song;
    }
    return nullptr;
}

std::string JukeboxBlockActor::soundOf(const Song &song) {
    return std::string("record.") + song.mDisc;
}

Tag JukeboxBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    if (!mRecord.isAir())
        data.put(TAG_RECORD_ITEM, ItemStackNbt::write(mRecord));
    return data;
}

// a loaded record counts as finished: the game does not resume it
void JukeboxBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    mFinishedRecording = true;
    mTicksPlaying = 0;
    mPlayedTicks = 0;
    mPending = Pending::None;
    mRecord = ItemStack::air();
    if (const Tag *record = data.get(TAG_RECORD_ITEM))
        mRecord = ItemStackNbt::read(*record, context);
}

Tag JukeboxBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    data.putByte(TAG_FINISHED_RECORDING, mFinishedRecording ? 1 : 0);
    data.putInt(TAG_TICKS_PLAYING, mTicksPlaying);
    if (!mRecord.isAir())
        data.put(TAG_RECORD_ITEM, ItemStackNbt::write(mRecord));
    return data;
}

const ItemStack &JukeboxBlockActor::getContainerItem(int slot) const {
    static const ItemStack AIR = ItemStack::air();
    return slot == 0 ? mRecord : AIR;
}

void JukeboxBlockActor::setContainerItem(int slot, ItemStack item) {
    if (slot == 0)
        setRecord(std::move(item), true);
}

bool JukeboxBlockActor::canHoldInSlot(int slot, const ItemStack &item) const {
    return slot == 0 && mRecord.isAir() && findSong(item) != nullptr;
}

bool JukeboxBlockActor::canTakeFromSlot(int slot) const {
    return slot == 0 && mFinishedRecording;
}

void JukeboxBlockActor::setRecord(ItemStack record, bool play) {
    const bool wasPlaying = isPlaying();
    if (!record.isAir())
        record.mCount = 1;
    mRecord = std::move(record);
    mTicksPlaying = 0;
    mPlayedTicks = 0;
    if (play && findSong(mRecord) != nullptr)
        mPending = Pending::Play;
    else
        mPending = wasPlaying ? Pending::Stop : Pending::None;
}

void JukeboxBlockActor::play(ServerNetworkHandler &owner) {
    mPending = Pending::None;
    const Song *song = findSong(mRecord);
    if (song == nullptr || mLevel == nullptr) {
        stop(owner);
        return;
    }

    mFinishedRecording = false;
    mTicksPlaying = 0;
    mPlayedTicks = 0;
    const BlockState state = mLevel->getBlockState(mPosition.x, mPosition.y, mPosition.z);
    owner.playBlockSound(*mLevel, soundOf(*song),
                         Vector3f((float) mPosition.x + 0.5f, (float) mPosition.y + 0.5f, (float) mPosition.z + 0.5f),
                         state.getHash());
    _changed(owner);
}

// no sound of its own: the client stops the disc when FinishedRecording comes back set
void JukeboxBlockActor::stop(ServerNetworkHandler &owner) {
    mPending = Pending::None;
    mFinishedRecording = true;
    _changed(owner);
}

void JukeboxBlockActor::eject(ServerNetworkHandler &owner) {
    _drop(owner);
    stop(owner);
}

int32_t JukeboxBlockActor::getComparatorSignal() const {
    const Song *song = findSong(mRecord);
    return song == nullptr ? 0 : song->mComparatorSignal;
}

bool JukeboxBlockActor::tick(ServerNetworkHandler &owner) {
    if (mLevel == nullptr)
        return true;
    const BlockState *state = mLevel->peekBlockPtr(mPosition.x, mPosition.y, mPosition.z);
    if (state == nullptr)
        return true;
    if (state->mName != BlockNames::JUKEBOX)
        return false;

    // the song's length counts from the tick after it starts
    if (mPending == Pending::Play) {
        play(owner);
        return true;
    }
    if (mPending == Pending::Stop)
        stop(owner);

    if (mFinishedRecording)
        return true;

    // the game ends the song when its sound runs out
    const Song *song = findSong(mRecord);
    if (song == nullptr || ++mPlayedTicks >= (int32_t) (song->mSeconds * TICKS_PER_SECOND))
        stop(owner);
    return true;
}

void JukeboxBlockActor::onRemoved() {
    ServerNetworkHandler *owner = mLevel == nullptr ? nullptr : mLevel->getOwner();
    if (owner == nullptr)
        return;
    _drop(*owner);
    mFinishedRecording = true;
    RedstoneSystem::updateAroundRedstone(*owner, *mLevel, mPosition);
}

void JukeboxBlockActor::_changed(ServerNetworkHandler &owner) {
    if (mLevel == nullptr)
        return;
    BlockActionHandler::broadcastBlockActorData(owner, *mLevel, *this);
    RedstoneSystem::updateAroundRedstone(owner, *mLevel, mPosition);
}

void JukeboxBlockActor::_drop(ServerNetworkHandler &owner) {
    if (mRecord.isAir() || mLevel == nullptr)
        return;

    // up, north, south, east, west, down: the first free block takes the record
    static const int SIDES[6][3] = {{0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {1, 0, 0}, {-1, 0, 0}, {0, -1, 0}};
    Vector3i target(mPosition.x, mPosition.y + 1, mPosition.z);
    for (const int *side: SIDES) {
        const Vector3i candidate(mPosition.x + side[0], mPosition.y + side[1], mPosition.z + side[2]);
        const BlockState *state = mLevel->peekBlockPtr(candidate.x, candidate.y, candidate.z);
        if (state != nullptr && state->mName == BlockNames::AIR) {
            target = candidate;
            break;
        }
    }

    const ItemStack record = mRecord;
    mRecord = ItemStack::air();
    mTicksPlaying = 0;
    mPlayedTicks = 0;
    ItemActorHandler::dropItem(owner, *mLevel, ItemActorHandler::randomPopPosition(target), record,
                               ItemActorHandler::randomDropMotion(), ItemActorHandler::DROP_PICKUP_DELAY);
}
