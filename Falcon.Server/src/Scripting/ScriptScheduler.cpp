#include "Scripting/ScriptScheduler.h"

#include "Core/Debug/BedrockLog.h"

#include <algorithm>

#include <quickjs.h>

namespace {
    JSValue asValue(void *callback) {
        return JS_MKPTR(JS_TAG_OBJECT, callback);
    }
}

ScriptScheduler::~ScriptScheduler() {
    clearAll();
}

uint32_t ScriptScheduler::_add(void *callback, int64_t delayTicks, int64_t periodTicks, bool repeating) {
    Task task;
    task.mId = mNextId++;
    task.mCallback = callback;
    task.mNextTick = mCurrentTick + (delayTicks < 1 ? 1 : delayTicks);
    task.mPeriodTicks = periodTicks < 1 ? 1 : periodTicks;
    task.mRepeating = repeating;
    mTasks.push_back(task);
    return task.mId;
}

uint32_t ScriptScheduler::run(void *callback) {
    return _add(callback, 1, 1, false);
}

uint32_t ScriptScheduler::runTimeout(void *callback, int64_t delayTicks) {
    return _add(callback, delayTicks, delayTicks, false);
}

uint32_t ScriptScheduler::runInterval(void *callback, int64_t periodTicks) {
    return _add(callback, periodTicks, periodTicks, true);
}

void ScriptScheduler::clear(uint32_t runId) {
    for (auto it = mTasks.begin(); it != mTasks.end(); ++it) {
        if (it->mId != runId)
            continue;

        JS_FreeValue(mContext, asValue(it->mCallback));
        mTasks.erase(it);
        return;
    }
}

void ScriptScheduler::tick(int64_t currentTick) {
    mCurrentTick = currentTick;
    if (!mInitialised) {
        mInitialised = true;
        return;
    }

    if (mTasks.empty())
        return;

    const std::vector<Task> snapshot = mTasks;

    for (const Task &task: snapshot) {
        if (task.mNextTick > currentTick)
            continue;

        bool stillActive = false;
        for (Task &live: mTasks) {
            if (live.mId != task.mId)
                continue;

            stillActive = true;
            if (live.mRepeating)
                live.mNextTick = currentTick + live.mPeriodTicks;
            break;
        }

        if (!stillActive)
            continue;

        JSValue callback = asValue(task.mCallback);
        JSValue result = JS_Call(mContext, callback, JS_UNDEFINED, 0, nullptr);

        if (JS_IsException(result)) {
            JSValue exception = JS_GetException(mContext);
            const char *message = JS_ToCString(mContext, exception);
            LOG_ERROR(LogAreaID::Script, "Script scheduled task error: %s", message == nullptr ? "?" : message);
            if (message != nullptr)
                JS_FreeCString(mContext, message);

            JSValue stack = JS_GetPropertyStr(mContext, exception, "stack");
            if (!JS_IsUndefined(stack)) {
                const char *trace = JS_ToCString(mContext, stack);
                if (trace != nullptr) {
                    LOG_ERROR(LogAreaID::Script, "%s", trace);
                    JS_FreeCString(mContext, trace);
                }
            }
            JS_FreeValue(mContext, stack);
            JS_FreeValue(mContext, exception);
        }

        JS_FreeValue(mContext, result);

        if (!task.mRepeating)
            clear(task.mId);
    }
}

void ScriptScheduler::clearAll() {
    for (const Task &task: mTasks)
        JS_FreeValue(mContext, asValue(task.mCallback));
    mTasks.clear();
}
