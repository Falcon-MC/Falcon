#pragma once

#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

class BlockState;
class Level;
class ScriptApi;
class ServerNetworkHandler;

// MinecraftGameTest: places the test structures, starts the tests in the pack that registered them, reports and
// runs the batches; the test code itself runs in the pack's script
class GameTestRunner {
public:
    // gameTest.error.structureTooBig: every side below 49 (sub_14BE80D30)
    static constexpr int32_t MAX_CREATE_SIZE = 48;
    static constexpr int32_t DEFAULT_CREATE_SIZE = 5;
    // runthis looks this far for a structure block, runthese and pos up to the second radius
    static constexpr int32_t NEAR_RADIUS = 15;
    static constexpr int32_t FAR_RADIUS = 200;
    // the batch runner's row length and the largest batch (0x6400000005 in sub_14BE80D30)
    static constexpr int32_t TESTS_PER_ROW = 5;
    static constexpr int32_t MAX_TESTS_PER_BATCH = 100;

    // a RegistrationBuilder as the pack reports it
    struct Descriptor {
        ScriptApi *mApi = nullptr;
        std::string mName;
        std::string mStructureName;
        std::string mBatch;
        std::vector<std::string> mTags;
        int32_t mMaxTicks = 100;
        int32_t mSetupTicks = 0;
        int32_t mPadding = 1;
        bool mRequired = true;
        int32_t mMaxAttempts = 1;
        int32_t mRequiredSuccessfulAttempts = 1;
        bool mRotateTest = false;
        bool mHasStructureLocation = false;
        Vector3i mStructureLocation;
        std::string mStructureDimension;
    };

    // the error a test failed with: a text key and its parameters
    struct Failure {
        std::string mMessage;
        std::vector<std::string> mParameters;
        bool mHasPosition = false;
        Vector3i mPosition;
    };

    // a command's answer: a text key with its parameters
    struct Outcome {
        bool mSuccess = false;
        std::string mKey;
        std::vector<std::string> mParameters;
    };

    void bindHost(ServerNetworkHandler &host) {
        mHost = &host;
    }

    // every registered test, the first registration of a name winning
    std::vector<Descriptor> collect() const;

    // the names and the tags for the command's soft enums
    std::vector<std::string> testNames() const;

    std::vector<std::string> knownTags() const;

    Outcome run(Level &level, const Vector3i &origin, const std::string &name, int32_t rotation, bool stopOnFailure,
                int32_t repeatCount);

    // the test of the nearest structure block within NEAR_RADIUS
    Outcome runThis(Level &level, const Vector3i &origin);

    // every structure block within FAR_RADIUS, each test in place
    Outcome runThese(Level &level, const Vector3i &origin);

    Outcome runSet(Level &level, const Vector3i &origin, const std::string &tag, int32_t rotation, bool untilFail);

    Outcome create(Level &level, const Vector3i &origin, const std::string &name, int32_t width, int32_t height,
                   int32_t depth);

    Outcome clearAll(Level &level);

    Outcome stopAll();

    // the structure block holding a looked at block: its name and the block relative to it
    Outcome position(Level &level, const Vector3i &target);

    // from the pack: the test is over, failure null when it passed
    void finished(int32_t id, const Failure *failure);

    void tick();

private:
    struct Tracker;

    struct Instance {
        int32_t mId = 0;
        Descriptor mDescriptor;
        Level *mLevel = nullptr;
        Vector3i mStructureBlock;
        int32_t mRotation = 0;
        Vector3i mSize;
        int32_t mAttempts = 0;
        int32_t mSuccesses = 0;
        int32_t mRepeatsLeft = 0;
        bool mStopOnFailure = false;
        bool mUntilFail = false;
        bool mRunning = false;
        std::shared_ptr<Tracker> mTracker;
        // the batch this instance belongs to, -1 for a test run alone
        int32_t mBatch = -1;
    };

    // the tests one command started, for the closing report
    struct Tracker {
        int32_t mTotal = 0;
        int32_t mDone = 0;
        int32_t mRequiredFailed = 0;
        int32_t mOptionalFailed = 0;
        bool mStopped = false;
    };

    struct Batch {
        std::string mName;
        std::string mCallbackName;
        ScriptApi *mApi = nullptr;
        std::vector<Descriptor> mTests;
    };

    struct BatchRun {
        std::vector<Batch> mBatches;
        size_t mNext = 0;
        Level *mLevel = nullptr;
        Vector3i mCorner;
        int32_t mRotation = 0;
        bool mUntilFail = false;
        std::shared_ptr<Tracker> mTracker;
        int32_t mRunning = 0;
        std::vector<int32_t> mInstances;
    };

    Instance &_make(const Descriptor &descriptor, Level &level, const Vector3i &structureBlock, int32_t rotation,
                    const std::shared_ptr<Tracker> &tracker);

    // places the structure and starts the test; false when the structure could not be spawned
    bool _spawn(Instance &instance);

    void _startBatch();

    void _passed(Instance &instance);

    void _failed(Instance &instance, const Failure &failure);

    // the instance is done for good: its tracker counts it
    void _complete(Instance &instance, bool passed);

    void _rerun(Instance &instance);

    void _beacon(Instance &instance, const char *glass);

    void _lectern(Instance &instance, const std::string &text);

    void _marker(Instance &instance, const Vector3i &position, const std::string &text);

    void _say(const char *color, const std::vector<std::pair<std::string, std::vector<std::string>>> &parts);

    void _report(const Tracker &tracker);

    void _setBlock(Level &level, const Vector3i &position, const std::string &identifier);

    void _setBlock(Level &level, const Vector3i &position, const BlockState &state);

    void _clearCells(Level &level, const std::vector<Vector3i> &cells);

    const Descriptor *_find(const std::string &name, const std::vector<Descriptor> &all) const;

    // the structure block at or around a position, null for none
    struct StructureBlockHit {
        Vector3i mPosition;
        std::string mName;
        int32_t mRotation = 0;
        Vector3i mOffset;
        Vector3i mSize;
    };

    bool _nearestStructureBlock(Level &level, const Vector3i &origin, int32_t radius, StructureBlockHit &hit) const;

    bool _containingStructureBlock(Level &level, const Vector3i &target, int32_t radius, StructureBlockHit &hit) const;

    ServerNetworkHandler *mHost = nullptr;
    int32_t mNextId = 1;
    bool mStartingBatch = false;
    std::map<int32_t, std::unique_ptr<Instance>> mInstances;
    std::vector<std::unique_ptr<BatchRun>> mBatchRuns;

    // GameTestInstanceLedger: every block a test placed, cleared by clearall
    struct Ledger {
        Level *mLevel = nullptr;
        std::vector<Vector3i> mCells;
    };
    std::vector<Ledger> mLedger;
};
