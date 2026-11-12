#include "Scripting/GameTestRunner.h"

#include "Core/Json/Json.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/ClientboundDebugRendererPacket.h"
#include "Protocol/Packets/TextPacket.h"
#include "Protocol/Packets/UpdateBlockPacket.h"
#include "Scripting/Binding/ScriptApi.h"
#include "Scripting/ScriptEngine.h"
#include "Scripting/VanillaTexts.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/CommandBlockActor.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
#include "World/Level/Block/Actor/StructureBlockActor.h"
#include "Protocol/BlockStateHasher.h"
#include "World/Level/Block/Systems/CommandBlockSystem.h"
#include "World/Level/Level.h"
#include "World/Level/Structure/McStructure.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <set>

namespace {
    std::string lower(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return (char) std::tolower(c); });
        return text;
    }

    bool sameName(const std::string &left, const std::string &right) {
        return lower(left) == lower(right);
    }

    // rawtext as tellraw sends it: the client translates every part
    std::string escape(const std::string &text) {
        std::string out;
        for (char c: text) {
            if (c == '"' || c == '\\')
                out.push_back('\\');
            if (c == '\n') {
                out += "\\n";
                continue;
            }
            out.push_back(c);
        }
        return out;
    }

    std::string format(const std::string &text, const std::vector<std::string> &parameters) {
        std::string out;
        size_t next = 0;
        for (size_t index = 0; index < text.size(); ++index) {
            if (text[index] == '%' && index + 1 < text.size() && text[index + 1] == 's') {
                out += next < parameters.size() ? parameters[next++] : std::string();
                ++index;
                continue;
            }
            out.push_back(text[index]);
        }
        return out;
    }

    // a key's english text when the admin added the vanilla pack, the key itself otherwise
    std::string text(const std::string &key, const std::vector<std::string> &parameters) {
        return format(VanillaTexts::getInstance().translate(key), parameters);
    }

    std::string number(int32_t value) {
        return std::to_string(value);
    }

    // StructureTemplate::getTransformedBounds: the box the rotated structure fills
    Vector3i rotatedSize(const Vector3i &size, int32_t rotation) {
        return (rotation & 1) != 0 ? Vector3i(size.z, size.y, size.x) : size;
    }

    // a cell of the structure as the rotation lands it, from the structure's corner
    Vector3i unrotate(const Vector3i &relative, int32_t rotation, const Vector3i &size) {
        const int32_t sx = size.x - 1, sz = size.z - 1;
        switch (rotation & 3) {
            case 1:
                return Vector3i(relative.z, relative.y, sz - relative.x);
            case 2:
                return Vector3i(sx - relative.x, relative.y, sz - relative.z);
            case 3:
                return Vector3i(sx - relative.z, relative.y, relative.x);
            default:
                return relative;
        }
    }
}

std::vector<GameTestRunner::Descriptor> GameTestRunner::collect() const {
    std::vector<Descriptor> all;
    if (mHost == nullptr)
        return all;
    std::set<std::string> seen;
    for (const std::unique_ptr<ScriptApi> &api: mHost->getScriptEngine().getApis()) {
        for (Descriptor &descriptor: api->gameTestList()) {
            if (seen.insert(lower(descriptor.mName)).second)
                all.push_back(std::move(descriptor));
        }
    }
    return all;
}

std::vector<std::string> GameTestRunner::testNames() const {
    std::vector<std::string> names;
    for (const Descriptor &descriptor: collect())
        names.push_back(descriptor.mName);
    return names;
}

std::vector<std::string> GameTestRunner::knownTags() const {
    std::vector<std::string> tags;
    if (mHost == nullptr)
        return tags;
    for (const std::unique_ptr<ScriptApi> &api: mHost->getScriptEngine().getApis()) {
        for (const std::string &tag: api->gameTestTags()) {
            if (std::find(tags.begin(), tags.end(), tag) == tags.end())
                tags.push_back(tag);
        }
    }
    return tags;
}

const GameTestRunner::Descriptor *GameTestRunner::_find(const std::string &name,
                                                        const std::vector<Descriptor> &all) const {
    for (const Descriptor &descriptor: all) {
        if (sameName(descriptor.mName, name))
            return &descriptor;
    }
    return nullptr;
}

GameTestRunner::Instance &GameTestRunner::_make(const Descriptor &descriptor, Level &level,
                                                const Vector3i &structureBlock, int32_t rotation,
                                                const std::shared_ptr<Tracker> &tracker) {
    std::unique_ptr<Instance> instance(new Instance());
    instance->mId = mNextId++;
    instance->mDescriptor = descriptor;
    instance->mLevel = &level;
    instance->mStructureBlock = structureBlock;
    instance->mRotation = rotation & 3;
    instance->mTracker = tracker;
    Instance &reference = *instance;
    mInstances[reference.mId] = std::move(instance);
    return reference;
}

void GameTestRunner::_setBlock(Level &level, const Vector3i &position, const std::string &identifier) {
    _setBlock(level, position, ScriptApi::defaultBlockState(identifier));
}

void GameTestRunner::_setBlock(Level &level, const Vector3i &position, const BlockState &state) {
    level.setBlockState(position.x, position.y, position.z, state);
    const BlockState placed = level.getBlockState(position.x, position.y, position.z);
    UpdateBlockPacket update;
    update.mBlockPosition = position;
    update.mRuntimeId = (uint32_t) BlockStateHasher::hash(placed.mName, placed.mStates);
    update.mFlags = UpdateBlockPacket::Flag::All;
    update.mDataLayer = 0;
    BlockActionHandler::broadcastToViewers(
            *mHost, level, Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f),
            update);
}

void GameTestRunner::_clearSpace(Level &level, const Vector3i &low, const Vector3i &high, int32_t floor) {
    const BlockState air = ScriptApi::defaultBlockState("minecraft:air");
    const BlockState dirt = ScriptApi::defaultBlockState("minecraft:dirt");
    const Vector3i from(low.x - 2, low.y - 3, low.z - 3), to(high.x + 3, high.y + 20, high.z + 3);
    for (int32_t x = from.x; x <= to.x; ++x) {
        for (int32_t y = from.y; y <= to.y; ++y) {
            for (int32_t z = from.z; z <= to.z; ++z) {
                if (level.peekBlockPtr(x, y, z) == nullptr)
                    continue;
                const Vector3i cell(x, y, z);
                level.getBlockActors().remove(cell);
                // the extra layer first, so no water stays behind
                const BlockState *extra = level.peekBlockPtr(x, y, z, 1);
                if (y >= floor && extra != nullptr && extra->mName != "minecraft:air") {
                    level.setBlockStateAtLayer(x, y, z, 1, air);
                    UpdateBlockPacket update;
                    update.mBlockPosition = cell;
                    update.mRuntimeId = (uint32_t) BlockStateHasher::hash(air.mName, air.mStates);
                    update.mFlags = UpdateBlockPacket::Flag::All;
                    update.mDataLayer = 1;
                    BlockActionHandler::broadcastToViewers(
                            *mHost, level, Vector3f((float) x + 0.5f, (float) y + 0.5f, (float) z + 0.5f), update);
                }
                _setBlock(level, cell, y >= floor ? air : dirt);
            }
        }
    }

    // every entity in the space but the players
    std::vector<int64_t> removed;
    const auto inside = [&from, &to](const Vector3f &position) {
        return position.x >= (float) from.x && position.x < (float) (to.x + 1) && position.y >= (float) from.y
               && position.y < (float) (to.y + 1) && position.z >= (float) from.z && position.z < (float) (to.z + 1);
    };
    for (auto &entry: mHost->getActors()) {
        if (entry.second->isIn(level) && inside(entry.second->getPosition()))
            removed.push_back(entry.first);
    }
    for (const int64_t id: removed)
        mHost->removeActor(id);
    for (std::unique_ptr<ItemActor> &item: mHost->getItemEntities()) {
        if (item != nullptr && item->isIn(level) && inside(item->getPosition()))
            item->setRemoved(true);
    }
}

// StructureUtils::spawnStructure (sub_144B27400): the structure over its structure block, a redstone block under
bool GameTestRunner::_spawn(Instance &instance) {
    ++instance.mAttempts;
    ScriptEngine &engine = mHost->getScriptEngine();
    std::shared_ptr<McStructure> structure = engine.structures().find(instance.mDescriptor.mStructureName);
    Level &level = *instance.mLevel;
    Vector3i &at = instance.mStructureBlock;
    if (structure != nullptr && at.y > level.getMaxY())
        at.y = level.getAboveTopSolidBlock(at.x, level.getMaxY(), at.z);
    if (structure == nullptr || at.y + 1 < level.getMinY() || at.y + structure->getSize().y > level.getMaxY()) {
        Failure failure;
        failure.mMessage = "gameTest.error.structureNotSpawned";
        failure.mParameters.push_back(instance.mDescriptor.mStructureName);
        _failed(instance, failure);
        return false;
    }
    instance.mSize = structure->getSize();

    McStructure::PlaceOptions options;
    options.mRotation = instance.mRotation;
    const Vector3i corner(at.x, at.y + 1, at.z);
    structure->place(*mHost, level, corner, options);

    // GameTestInstanceLedger::testStructureLoaded (sub_144B234B0): the loaded structure's bounds
    const Vector3i box = rotatedSize(instance.mSize, instance.mRotation);
    Ledger ledger;
    ledger.mLevel = &level;
    ledger.mLow = corner;
    ledger.mHigh = Vector3i(corner.x + box.x - 1, corner.y + box.y - 1, corner.z + box.z - 1);
    mLedger.push_back(ledger);

    _setBlock(level, Vector3i(at.x, at.y - 1, at.z), "minecraft:redstone_block");
    _setBlock(level, at, "minecraft:structure_block");
    // sub_144B27160: load mode, the structure's size, rotation and a centred pivot, offset (0, 1, 0)
    StructureBlockActor &block = level.getBlockActors().getOrCreate<StructureBlockActor>(at);
    block.mMode = StructureBlockActor::Mode::Load;
    block.mStructureName = instance.mDescriptor.mStructureName;
    block.mSize = instance.mSize;
    block.mOffset = Vector3i(0, 1, 0);
    block.mRotation = (uint8_t) instance.mRotation;
    block.mShowBoundingBox = true;
    BlockActionHandler::broadcastBlockActorData(*mHost, level, block);

    // StructureUtils::addCommandBlockAndButtonToStartTest (sub_144B278E0): (1, 0, -1), the button north of it
    const Vector3i commandBlock(at.x + 1, at.y, at.z - 1);
    _setBlock(level, commandBlock, "minecraft:command_block");
    CommandBlockActor &command = CommandBlockSystem::getOrCreate(level, commandBlock);
    command.setCommand("gametest runthis");
    command.mCustomName = instance.mDescriptor.mName;
    CommandBlockSystem::broadcastData(*mHost, level, command);
    const Vector3i button(commandBlock.x, commandBlock.y, commandBlock.z - 1);
    BlockState stone = ScriptApi::defaultBlockState("minecraft:stone_button");
    stone.mStates.putInt("facing_direction", 2);
    _setBlock(level, button, stone);

    instance.mRunning = true;
    ScriptApi *api = instance.mDescriptor.mApi;
    if (api == nullptr
        || !api->gameTestStart(instance.mId, instance.mDescriptor.mName, at, instance.mRotation, instance.mSize,
                               level)) {
        instance.mRunning = false;
        Failure failure;
        failure.mMessage = "gameTest.error.testMissing";
        failure.mParameters.push_back(instance.mDescriptor.mName);
        _failed(instance, failure);
        return false;
    }
    return true;
}

void GameTestRunner::_say(const char *color,
                          const std::vector<std::pair<std::string, std::vector<std::string>>> &parts) {
    std::string json = "{\"rawtext\":[{\"text\":\"" + escape(color) + "\"}";
    for (const auto &part: parts) {
        if (part.first.empty())
            continue;
        if (part.first == " ") {
            json += ",{\"text\":\" \"}";
            continue;
        }
        json += ",{\"translate\":\"" + escape(part.first) + "\",\"with\":[";
        for (size_t index = 0; index < part.second.size(); ++index)
            json += (index == 0 ? "\"" : ",\"") + escape(part.second[index]) + "\"";
        json += "]}";
    }
    json += "]}";

    TextPacket packet;
    packet.mType = TextPacket::Type::Json;
    packet.mNeedsTranslation = true;
    packet.mMessage = json;
    for (auto &entry: mHost->getPlayers()) {
        if (entry.second.isSpawned())
            mHost->sendPacketTo(entry.second.getNetworkIdentifier(), packet);
    }
}

// ReportGameListener::spawnBeacon (sub_144B08F80): the beacon a block below and west-north of the structure block,
// the glass over it, iron under it
void GameTestRunner::_beacon(Instance &instance, const char *glass) {
    Level &level = *instance.mLevel;
    const Vector3i beacon(instance.mStructureBlock.x - 1, instance.mStructureBlock.y - 1,
                          instance.mStructureBlock.z - 1);
    _setBlock(level, beacon, "minecraft:beacon");
    _setBlock(level, Vector3i(beacon.x, beacon.y + 1, beacon.z), glass);
    for (int32_t x = -1; x <= 1; ++x) {
        for (int32_t z = -1; z <= 1; ++z)
            _setBlock(level, Vector3i(beacon.x + x, beacon.y - 1, beacon.z + z), "minecraft:iron_block");
    }
}

// the failure report on a lectern west, up and north of the structure block (sub_144B0E160)
void GameTestRunner::_lectern(Instance &instance, const std::string &page) {
    Level &level = *instance.mLevel;
    const Vector3i position(instance.mStructureBlock.x - 1, instance.mStructureBlock.y + 1,
                            instance.mStructureBlock.z - 1);
    _setBlock(level, position, "minecraft:lectern");
    ItemStack book = mHost->createItemStack("minecraft:written_book", 1);
    if (!book.isAir()) {
        Tag pages = Tag::ofList(Tag::Type::Compound);
        Tag entry = Tag::ofCompound();
        entry.putString("text", page);
        entry.putString("photoname", "");
        pages.addToList(std::move(entry));
        Tag tag = Tag::ofCompound();
        tag.put("pages", std::move(pages));
        tag.putString("title", text("gameTest.report.bookTitle", {}));
        tag.putString("author", "GameTest");
        tag.putInt("generation", 0);
        book.mTag = std::move(tag);
        LecternBlockActor &lectern = level.getBlockActors().getOrCreate<LecternBlockActor>(position);
        lectern.setBook(std::move(book));
        BlockActionHandler::broadcastBlockActorData(*mHost, level, lectern);
    }
}

// ClientboundDebugRendererPacket: a red cube for a day where the error happened
void GameTestRunner::_marker(Instance &instance, const Vector3i &position, const std::string &message) {
    (void) instance;
    ClientboundDebugRendererPacket packet;
    packet.mDebugMarkerType = ClientboundDebugRendererType::AddDebugMarkerCube;
    packet.mMarkerText = message;
    packet.mMarkerPosition = Vector3f((float) position.x, (float) position.y, (float) position.z);
    packet.mMarkerColorRed = 1.0f;
    packet.mMarkerColorGreen = 0.0f;
    packet.mMarkerColorBlue = 0.0f;
    packet.mMarkerColorAlpha = 1.0f;
    packet.mMarkerDuration = 86400000;
    for (auto &entry: mHost->getPlayers()) {
        if (entry.second.isSpawned())
            mHost->sendPacketTo(entry.second.getNetworkIdentifier(), packet);
    }
}

void GameTestRunner::_rerun(Instance &instance) {
    instance.mRunning = false;
    _spawn(instance);
}

// ReportGameListener::testPassed (sub_144B09610), the flaky branch as GameTest's java side runs it
void GameTestRunner::_passed(Instance &instance) {
    instance.mRunning = false;
    ++instance.mSuccesses;
    const Descriptor &test = instance.mDescriptor;
    const bool flaky = test.mMaxAttempts > 1 || test.mRequiredSuccessfulAttempts > 1;
    if (!flaky) {
        _beacon(instance, "minecraft:lime_stained_glass");
        _say("§a", {{"gameTest.success.passed", {test.mName}}});
        if (instance.mUntilFail || instance.mRepeatsLeft > 0) {
            if (instance.mRepeatsLeft > 0)
                --instance.mRepeatsLeft;
            _rerun(instance);
            return;
        }
        _complete(instance, true);
        return;
    }
    if (instance.mSuccesses >= test.mRequiredSuccessfulAttempts) {
        _beacon(instance, "minecraft:lime_stained_glass");
        _say("§a", {{"gameTest.success.flakyPassedEnough",
                     {test.mName, number(instance.mSuccesses), number(instance.mAttempts)}}});
        _complete(instance, true);
        return;
    }
    _say("§a",
         {{"gameTest.success.flakyPassed", {test.mName, number(instance.mAttempts), number(instance.mSuccesses)}}});
    _rerun(instance);
}

// ReportGameListener::testFailed (sub_144B0E160)
void GameTestRunner::_failed(Instance &instance, const Failure &failure) {
    instance.mRunning = false;
    const Descriptor &test = instance.mDescriptor;
    const bool flaky = test.mMaxAttempts > 1 || test.mRequiredSuccessfulAttempts > 1;
    if (flaky) {
        std::vector<std::pair<std::string, std::vector<std::string>>> parts = {
                {"gameTest.error.flakyFailed", {test.mName, number(instance.mAttempts), number(test.mMaxAttempts)}}};
        if (test.mRequiredSuccessfulAttempts > 1)
            parts.push_back({"gameTest.error.flakyFailedShowSuccesses",
                             {number(instance.mSuccesses), number(test.mRequiredSuccessfulAttempts)}});
        _say("§e", parts);
        if (test.mMaxAttempts - instance.mAttempts + instance.mSuccesses >= test.mRequiredSuccessfulAttempts) {
            _rerun(instance);
            return;
        }
        Failure exhausted;
        exhausted.mMessage = "gameTest.error.flakyTestError";
        exhausted.mParameters = {number(instance.mSuccesses), number(instance.mAttempts),
                                 number(test.mRequiredSuccessfulAttempts), number(test.mMaxAttempts)};
        _beacon(instance, test.mRequired ? "minecraft:red_stained_glass" : "minecraft:orange_stained_glass");
        _say(test.mRequired ? "§c" : "§e",
             {{"gameTest.error.failed", {test.mName}}, {exhausted.mMessage, exhausted.mParameters}});
        _lectern(instance, test.mName + "\n" + text(exhausted.mMessage, exhausted.mParameters));
        _complete(instance, false);
        return;
    }

    _beacon(instance, test.mRequired ? "minecraft:red_stained_glass" : "minecraft:orange_stained_glass");
    // the failed line, a space, then the error
    _say(test.mRequired ? "§c" : "§e",
         {{"gameTest.error.failed", {test.mName}}, {" ", {}}, {failure.mMessage, failure.mParameters}});
    std::string page = (test.mRequired ? std::string() : text("gameTest.optionalPrefix", {}) + " ") + test.mName + "\n"
                       + text(failure.mMessage, failure.mParameters);
    if (failure.mHasPosition) {
        const Vector3i relative = unrotate(Vector3i(failure.mPosition.x - instance.mStructureBlock.x,
                                                    failure.mPosition.y - instance.mStructureBlock.y,
                                                    failure.mPosition.z - instance.mStructureBlock.z),
                                           instance.mRotation, instance.mSize);
        page += "\n"
                + text("gameTest.report.testLocation",
                       {number(failure.mPosition.x), number(failure.mPosition.y), number(failure.mPosition.z),
                        number(relative.x), number(relative.y), number(relative.z)});
        _marker(instance, failure.mPosition, text(failure.mMessage, failure.mParameters));
    }
    _lectern(instance, page);
    if (instance.mRepeatsLeft > 0 && !instance.mStopOnFailure) {
        --instance.mRepeatsLeft;
        _rerun(instance);
        return;
    }
    _complete(instance, false);
}

void GameTestRunner::_complete(Instance &instance, bool passed) {
    const int32_t id = instance.mId;
    std::shared_ptr<Tracker> tracker = instance.mTracker;
    const bool required = instance.mDescriptor.mRequired;
    const int32_t batch = instance.mBatch;
    mInstances.erase(id);

    if (tracker != nullptr && !tracker->mStopped) {
        ++tracker->mDone;
        if (!passed)
            ++(required ? tracker->mRequiredFailed : tracker->mOptionalFailed);
    }

    for (std::unique_ptr<BatchRun> &run: mBatchRuns) {
        auto found = std::find(run->mInstances.begin(), run->mInstances.end(), id);
        if (found == run->mInstances.end())
            continue;
        run->mInstances.erase(found);
        if (!run->mInstances.empty())
            break;
        const Batch &done = run->mBatches[run->mNext - 1];
        if (done.mApi != nullptr)
            done.mApi->gameTestBatch(false, done.mCallbackName);
        (void) batch;
        break;
    }
    _startBatch();

    if (tracker != nullptr && !tracker->mStopped && tracker->mDone >= tracker->mTotal) {
        tracker->mStopped = true;
        _report(*tracker);
    }
}

// the closing summary (sub_144B133C0)
void GameTestRunner::_report(const Tracker &tracker) {
    _say("§f", {{"gameTest.report.testCount", {number(tracker.mTotal)}}});
    if (tracker.mRequiredFailed > 0)
        _say("§c", {{"gameTest.report.someTestsFailed", {number(tracker.mRequiredFailed)}}});
    else
        _say("§a", {{"gameTest.report.allRequiredTestsPassed", {}}});
    if (tracker.mOptionalFailed > 0)
        _say("§7", {{"gameTest.report.optionalFailures", {number(tracker.mOptionalFailed)}}});
}

// GameTestBatchRunner::runBatch (sub_144B157F0): the next batch's tests in rows of TESTS_PER_ROW
void GameTestRunner::_startBatch() {
    // a spawn that fails completes its test, which comes back here
    if (mStartingBatch)
        return;
    mStartingBatch = true;
    for (auto it = mBatchRuns.begin(); it != mBatchRuns.end();) {
        BatchRun &run = **it;
        if (!run.mInstances.empty()) {
            ++it;
            continue;
        }
        if (run.mNext >= run.mBatches.size()) {
            it = mBatchRuns.erase(it);
            continue;
        }
        const Batch &batch = run.mBatches[run.mNext++];
        if (batch.mApi != nullptr)
            batch.mApi->gameTestBatch(true, batch.mCallbackName);

        int32_t x = run.mCorner.x, z = run.mCorner.z, rowDepth = 0, placed = 0;
        std::vector<Instance *> started;
        for (size_t index = 0; index < batch.mTests.size(); ++index) {
            const Descriptor &test = batch.mTests[index];
            const int32_t rotation = batch.mRotations[index];
            Vector3i at(x, run.mCorner.y, z);
            Level *level = run.mLevel;
            if (test.mHasStructureLocation) {
                at = test.mStructureLocation;
            } else {
                std::shared_ptr<McStructure> structure = mHost->getScriptEngine().structures().find(
                        test.mStructureName);
                const Vector3i size = structure == nullptr ? Vector3i(1, 1, 1)
                                                           : rotatedSize(structure->getSize(), rotation);
                const int32_t padding = std::max(test.mPadding, 0);
                at = Vector3i(x + padding, run.mCorner.y, z + padding);
                rowDepth = std::max(rowDepth, size.z - 1 + 2 * padding + 7);
                if (++placed % TESTS_PER_ROW != 0) {
                    x += size.x - 1 + 2 * padding + 6;
                } else {
                    x = run.mCorner.x;
                    z += rowDepth;
                    rowDepth = 0;
                }
            }
            Instance &instance = _make(test, *level, at, rotation, run.mTracker);
            instance.mUntilFail = run.mUntilFail;
            instance.mBatch = (int32_t) run.mNext - 1;
            run.mInstances.push_back(instance.mId);
            started.push_back(&instance);
        }
        for (Instance *instance: started)
            _spawn(*instance);
        ++it;
    }
    mStartingBatch = false;
}

GameTestRunner::Outcome GameTestRunner::run(Level &level, const Vector3i &origin, const std::string &name,
                                            int32_t rotation, bool stopOnFailure, int32_t repeatCount) {
    if (repeatCount <= 0)
        return Outcome{false, "gameTest.error.repeatCountTooSmall", {}};
    const std::vector<Descriptor> all = collect();
    const Descriptor *test = _find(name, all);
    if (test == nullptr)
        return Outcome{false, "gameTest.error.testMissing", {name}};

    std::shared_ptr<Tracker> tracker = std::make_shared<Tracker>();
    tracker->mTotal = 1;
    Level *target = &level;
    // a position over the build limit asks the spawn for the ground under it (sub_144AEAB40)
    Vector3i at(origin.x, level.getMaxY() + 1, origin.z + 3);
    if (test->mHasStructureLocation)
        at = test->mStructureLocation;
    Instance &instance = _make(*test, *target, at, rotation, tracker);
    instance.mStopOnFailure = stopOnFailure;
    instance.mRepeatsLeft = repeatCount - 1;
    _spawn(instance);
    return Outcome{true, "", {}};
}

bool GameTestRunner::_nearestStructureBlock(Level &level, const Vector3i &origin, int32_t radius,
                                            StructureBlockHit &hit) const {
    bool found = false;
    int64_t best = 0;
    for (StructureBlockActor *block: level.getBlockActors().findAll<StructureBlockActor>()) {
        const Vector3i &position = block->getPosition();
        const int64_t dx = position.x - origin.x, dy = position.y - origin.y, dz = position.z - origin.z;
        if (std::abs(dx) > radius || std::abs(dy) > radius || std::abs(dz) > radius)
            continue;
        const int64_t distance = dx * dx + dy * dy + dz * dz;
        if (found && distance >= best)
            continue;
        found = true;
        best = distance;
        hit.mPosition = position;
        hit.mName = block->mStructureName;
        hit.mRotation = block->mRotation & 3;
        hit.mOffset = block->mOffset;
        hit.mSize = block->mSize;
    }
    return found;
}

bool GameTestRunner::_containingStructureBlock(Level &level, const Vector3i &target, int32_t radius,
                                               StructureBlockHit &hit) const {
    for (StructureBlockActor *block: level.getBlockActors().findAll<StructureBlockActor>()) {
        const Vector3i &position = block->getPosition();
        if (std::abs(position.x - target.x) > radius || std::abs(position.y - target.y) > radius
            || std::abs(position.z - target.z) > radius)
            continue;
        const Vector3i size = rotatedSize(block->mSize, block->mRotation & 3);
        const Vector3i low(position.x + block->mOffset.x, position.y + block->mOffset.y, position.z + block->mOffset.z);
        if (target.x < low.x || target.y < low.y || target.z < low.z || target.x >= low.x + size.x
            || target.y >= low.y + size.y || target.z >= low.z + size.z)
            continue;
        hit.mPosition = position;
        hit.mName = block->mStructureName;
        hit.mRotation = block->mRotation & 3;
        hit.mOffset = block->mOffset;
        hit.mSize = block->mSize;
        return true;
    }
    return false;
}

GameTestRunner::Outcome GameTestRunner::runThis(Level &level, const Vector3i &origin) {
    StructureBlockHit hit;
    if (!_nearestStructureBlock(level, origin, NEAR_RADIUS, hit))
        return Outcome{false, "gameTest.error.missingStructure", {number(NEAR_RADIUS)}};
    const std::vector<Descriptor> all = collect();
    const Descriptor *test = _find(hit.mName, all);
    if (test == nullptr)
        return Outcome{false, "gameTest.error.testMissing", {hit.mName}};
    std::shared_ptr<Tracker> tracker = std::make_shared<Tracker>();
    tracker->mTotal = 1;
    Instance &instance = _make(*test, level, hit.mPosition, hit.mRotation, tracker);
    _spawn(instance);
    return Outcome{true, "", {}};
}

GameTestRunner::Outcome GameTestRunner::runThese(Level &level, const Vector3i &origin) {
    std::vector<StructureBlockHit> hits;
    for (StructureBlockActor *block: level.getBlockActors().findAll<StructureBlockActor>()) {
        const Vector3i &position = block->getPosition();
        if (std::abs(position.x - origin.x) > FAR_RADIUS || std::abs(position.y - origin.y) > FAR_RADIUS
            || std::abs(position.z - origin.z) > FAR_RADIUS)
            continue;
        StructureBlockHit hit;
        hit.mPosition = position;
        hit.mName = block->mStructureName;
        hit.mRotation = block->mRotation & 3;
        hits.push_back(hit);
    }
    if (hits.empty())
        return Outcome{false, "gameTest.error.missingStructure", {number(FAR_RADIUS)}};

    const std::vector<Descriptor> all = collect();
    std::shared_ptr<Tracker> tracker = std::make_shared<Tracker>();
    std::vector<Instance *> started;
    for (const StructureBlockHit &hit: hits) {
        const Descriptor *test = _find(hit.mName, all);
        if (test == nullptr)
            continue;
        started.push_back(&_make(*test, level, hit.mPosition, hit.mRotation, tracker));
    }
    tracker->mTotal = (int32_t) started.size();
    for (Instance *instance: started)
        _spawn(*instance);
    return Outcome{true, "gameTest.success.runningTests", {number((int32_t) hits.size())}};
}

GameTestRunner::Outcome GameTestRunner::runSet(Level &level, const Vector3i &origin, const std::string &tagName,
                                               int32_t rotation, bool untilFail) {
    const std::string tag = tagName.empty() ? "suite:default" : tagName;
    std::vector<Descriptor> tagged;
    for (const Descriptor &test: collect()) {
        for (const std::string &candidate: test.mTags) {
            if (sameName(candidate, tag)) {
                tagged.push_back(test);
                break;
            }
        }
    }
    if (tagged.empty())
        return Outcome{false, "gameTest.error.noTestsFound", {tag}};

    // GameTestRunner::groupTestsIntoBatches (sub_144AEC160): by pack and batch name, MAX_TESTS_PER_BATCH at most
    std::unique_ptr<BatchRun> run(new BatchRun());
    run->mLevel = &level;
    run->mCorner = Vector3i(origin.x, level.getAboveTopSolidBlock(origin.x, level.getMaxY(), origin.z), origin.z + 3);
    run->mRotation = rotation & 3;
    run->mUntilFail = untilFail;
    run->mTracker = std::make_shared<Tracker>();
    // a rotateTest test runs once per rotation: the started count adds three for it (sub_14BE848F0)
    std::vector<std::pair<std::pair<ScriptApi *, std::string>, std::vector<std::pair<Descriptor, int32_t>>>> groups;
    int32_t started = 0;
    for (const Descriptor &test: tagged) {
        auto group = std::find_if(groups.begin(), groups.end(), [&test](const auto &entry) {
            return entry.first.first == test.mApi && entry.first.second == test.mBatch;
        });
        if (group == groups.end()) {
            groups.push_back({{test.mApi, test.mBatch}, {}});
            group = groups.end() - 1;
        }
        if (test.mRotateTest) {
            for (int32_t step = 0; step < 4; ++step)
                group->second.emplace_back(test, step);
        } else {
            group->second.emplace_back(test, run->mRotation);
        }
        started += test.mRotateTest ? 4 : 1;
    }
    run->mTracker->mTotal = started;
    for (auto &group: groups) {
        for (size_t first = 0, index = 0; first < group.second.size(); first += MAX_TESTS_PER_BATCH, ++index) {
            Batch batch;
            batch.mApi = group.first.first;
            batch.mCallbackName = group.first.second;
            batch.mName = group.first.second + ":" + std::to_string(index);
            const size_t last = std::min(group.second.size(), first + (size_t) MAX_TESTS_PER_BATCH);
            for (size_t index = first; index < last; ++index) {
                batch.mTests.push_back(group.second[index].first);
                batch.mRotations.push_back(group.second[index].second);
            }
            run->mBatches.push_back(std::move(batch));
        }
    }
    mBatchRuns.push_back(std::move(run));
    _startBatch();
    return Outcome{true, "gameTest.success.testsStarted", {number(started), tag}};
}

// StructureUtils::createNewEmptyStructureBlock (sub_144B28530) and the polished andesite floor of sub_14BE80D30
GameTestRunner::Outcome GameTestRunner::create(Level &level, const Vector3i &origin, const std::string &name,
                                               int32_t width, int32_t height, int32_t depth) {
    if (height == 0)
        height = width;
    if (depth == 0)
        depth = width;
    if (width > MAX_CREATE_SIZE || height > MAX_CREATE_SIZE || depth > MAX_CREATE_SIZE)
        return Outcome{false, "gameTest.error.structureTooBig", {number(MAX_CREATE_SIZE)}};

    const Vector3i at(origin.x, level.getAboveTopSolidBlock(origin.x, level.getMaxY(), origin.z), origin.z + 3);
    _setBlock(level, at, "minecraft:structure_block");
    StructureBlockActor &block = level.getBlockActors().getOrCreate<StructureBlockActor>(at);
    block.mMode = StructureBlockActor::Mode::Save;
    block.mStructureName = name;
    block.mSize = Vector3i(width, height, depth);
    block.mOffset = Vector3i(0, 1, 0);
    block.mShowBoundingBox = true;
    BlockActionHandler::broadcastBlockActorData(*mHost, level, block);
    for (int32_t x = 0; x < width; ++x) {
        for (int32_t z = 0; z < depth; ++z)
            _setBlock(level, Vector3i(at.x + x, at.y + 1, at.z + z), "minecraft:polished_andesite");
    }

    const Vector3i commandBlock(at.x + 1, at.y, at.z - 1);
    _setBlock(level, commandBlock, "minecraft:command_block");
    CommandBlockActor &command = CommandBlockSystem::getOrCreate(level, commandBlock);
    command.setCommand("gametest runthis");
    command.mCustomName = name;
    CommandBlockSystem::broadcastData(*mHost, level, command);
    BlockState stone = ScriptApi::defaultBlockState("minecraft:stone_button");
    stone.mStates.putInt("facing_direction", 2);
    const Vector3i button(commandBlock.x, commandBlock.y, commandBlock.z - 1);
    _setBlock(level, button, stone);
    return Outcome{true, "", {}};
}

GameTestRunner::Outcome GameTestRunner::clearAll(Level &level) {
    ClientboundDebugRendererPacket clear;
    clear.mDebugMarkerType = ClientboundDebugRendererType::ClearDebugMarkers;
    for (auto &entry: mHost->getPlayers()) {
        if (entry.second.isSpawned())
            mHost->sendPacketTo(entry.second.getNetworkIdentifier(), clear);
    }
    // MinecraftGameTest::clearAllTests (sub_144AEBE20): the running tests go, then the space of every structure
    // the ledger holds in this dimension, its floor the structure's lowest layer
    stopAll();
    std::vector<Ledger> cleared;
    for (auto it = mLedger.begin(); it != mLedger.end();) {
        if (it->mLevel != &level) {
            ++it;
            continue;
        }
        cleared.push_back(*it);
        it = mLedger.erase(it);
    }
    for (const Ledger &entry: cleared)
        _clearSpace(level, entry.mLow, entry.mHigh, entry.mLow.y);
    return Outcome{true, "", {}};
}

GameTestRunner::Outcome GameTestRunner::stopAll() {
    std::vector<std::pair<ScriptApi *, int32_t>> running;
    for (auto &entry: mInstances)
        running.emplace_back(entry.second->mDescriptor.mApi, entry.first);
    mInstances.clear();
    mBatchRuns.clear();
    for (auto &entry: running) {
        if (entry.first != nullptr)
            entry.first->gameTestStop(entry.second);
    }
    return Outcome{true, "", {}};
}

GameTestRunner::Outcome GameTestRunner::position(Level &level, const Vector3i &target) {
    StructureBlockHit hit;
    if (!_containingStructureBlock(level, target, NEAR_RADIUS, hit)
        && !_containingStructureBlock(level, target, FAR_RADIUS, hit))
        return Outcome{false,
                       "gameTest.error.cannotFindStuctureBlockContainingBlock",
                       {number(target.x), number(target.y), number(target.z)}};
    const Vector3i relative = unrotate(
            Vector3i(target.x - hit.mPosition.x, target.y - hit.mPosition.y, target.z - hit.mPosition.z), hit.mRotation,
            hit.mSize);
    const std::string where = number(relative.x) + ", " + number(relative.y) + ", " + number(relative.z);
    // a green cube on the block for ten seconds, the relative position over it (sub_14BE80D30)
    ClientboundDebugRendererPacket marker;
    marker.mDebugMarkerType = ClientboundDebugRendererType::AddDebugMarkerCube;
    marker.mMarkerText = where;
    marker.mMarkerPosition = Vector3f((float) target.x, (float) target.y, (float) target.z);
    marker.mMarkerColorRed = 0.0f;
    marker.mMarkerColorGreen = 1.0f;
    marker.mMarkerColorBlue = 0.0f;
    marker.mMarkerColorAlpha = 1.0f;
    marker.mMarkerDuration = 10000;
    for (auto &entry: mHost->getPlayers()) {
        if (entry.second.isSpawned())
            mHost->sendPacketTo(entry.second.getNetworkIdentifier(), marker);
    }
    return Outcome{true, "gameTest.success.relativePosition", {hit.mName, where}};
}

void GameTestRunner::finished(int32_t id, const Failure *failure) {
    auto found = mInstances.find(id);
    if (found == mInstances.end() || !found->second->mRunning)
        return;
    Instance &instance = *found->second;
    if (failure == nullptr)
        _passed(instance);
    else
        _failed(instance, *failure);
}

void GameTestRunner::tick() {
    _startBatch();
}
