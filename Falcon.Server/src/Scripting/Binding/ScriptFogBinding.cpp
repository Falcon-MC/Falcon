#include "Scripting/Binding/ScriptApi.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Player/FogStack.h"

#include <algorithm>
#include <cstdint>
#include <list>

using namespace ScriptValues;

namespace {
    JSValue typeError(JSContext *ctx) {
        return JS_ThrowTypeError(ctx, "Native type conversion failed.");
    }

    // FogSettings resolves its player on every call: a missing one names the method
    ServerPlayer *fogOwner(JSContext *ctx, JSValueConst thisVal, const char *method, JSValue &error) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptFogRef *fog = api->native<ScriptFogRef>(thisVal);
        ServerPlayer *player = fog == nullptr ? nullptr
                                              : dynamic_cast<ServerPlayer *>(api->resolveEntity(fog->mEntity));
        if (player == nullptr)
            error = fog == nullptr ? typeError(ctx) : api->throwInvalidEntity("call function", method, fog->mEntity);
        return player;
    }

    JSValue fogError(JSContext *ctx, bool invalidId) {
        return ScriptApi::fromContext(ctx)->throwError(
                "FogSettingsError", invalidId ? "Failed to modify fog settings due to an invalid fog identifier."
                                              : "Failed to modify fog settings because the limit of 16 fog settings "
                                                "has been reached.");
    }

    // an absent tag files the entry under "untagged"
    bool readTag(JSContext *ctx, int argc, JSValueConst *argv, int index, std::string &tag, bool &present) {
        present = argc > index && !JS_IsUndefined(argv[index]);
        if (!present) {
            tag = FogStack::UNTAGGED;
            return true;
        }
        if (!JS_IsString(argv[index]))
            return false;
        tag = toString(ctx, argv[index]);
        return true;
    }

    void sendFog(JSContext *ctx, ServerPlayer &player) {
        FogStack::send(ScriptApi::fromContext(ctx)->host(), player);
    }

    uint64_t fnv1a(const std::string &text) {
        uint64_t hash = 0xCBF29CE484222325ull;
        for (unsigned char c: text)
            hash = (hash ^ c) * 0x100000001B3ull;
        return hash;
    }

    // getTags walks a std::unordered_set<std::string>: this replays its insertion and rehash
    // order so the tags come out as the game lists them
    class TagSet {
    public:
        void insert(const std::string &tag) {
            if (contains(tag))
                return;
            if (mOrder.size() + 1 > mBuckets.size())
                rehash(mBuckets.size() < 512 ? mBuckets.size() * 8 : mBuckets.size() * 2);
            place(mOrder.insert(mOrder.end(), tag));
        }

        const std::list<std::string> &order() const {
            return mOrder;
        }

    private:
        using Node = std::list<std::string>::iterator;
        struct Bucket {
            bool mUsed = false;
            Node mLo;
            Node mHi;
        };

        std::list<std::string> mOrder;
        std::vector<Bucket> mBuckets = std::vector<Bucket>(8);

        bool contains(const std::string &tag) const {
            return std::find(mOrder.begin(), mOrder.end(), tag) != mOrder.end();
        }

        // an empty bucket takes the node where it is, a used one gets it in front of its first node
        void place(Node node) {
            Bucket &bucket = mBuckets[fnv1a(*node) & (mBuckets.size() - 1)];
            if (!bucket.mUsed) {
                bucket.mUsed = true;
                bucket.mLo = node;
                bucket.mHi = node;
                return;
            }
            mOrder.splice(bucket.mLo, mOrder, node);
            bucket.mLo = node;
        }

        void rehash(size_t buckets) {
            mBuckets.assign(buckets, Bucket());
            for (Node node = mOrder.begin(); node != mOrder.end();) {
                const Node next = std::next(node);
                place(node);
                node = next;
            }
        }
    };

    JSValue fogGetStack(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        JSValue error = JS_UNDEFINED;
        ServerPlayer *player = fogOwner(ctx, thisVal, "getStack", error);
        if (player == nullptr)
            return error;
        JSValue list = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const ServerPlayer::FogEntry &entry: player->getFogStack())
            JS_SetPropertyUint32(ctx, list, index++, string(ctx, entry.mId));
        return list;
    }

    JSValue fogGetTags(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        JSValue error = JS_UNDEFINED;
        ServerPlayer *player = fogOwner(ctx, thisVal, "getTags", error);
        if (player == nullptr)
            return error;
        TagSet tags;
        for (const ServerPlayer::FogEntry &entry: player->getFogStack())
            tags.insert(entry.mTag);
        JSValue list = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const std::string &tag: tags.order())
            JS_SetPropertyUint32(ctx, list, index++, string(ctx, tag));
        return list;
    }

    // the identifier first, then the limit
    JSValue fogPush(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        std::string tag;
        bool tagged = false;
        if (argc < 1 || !JS_IsString(argv[0]) || !readTag(ctx, argc, argv, 1, tag, tagged))
            return typeError(ctx);
        JSValue error = JS_UNDEFINED;
        ServerPlayer *player = fogOwner(ctx, thisVal, "push", error);
        if (player == nullptr)
            return error;
        const std::string id = toString(ctx, argv[0]);
        if (!FogStack::isValidId(id))
            return fogError(ctx, true);
        FogStack::Entries &stack = player->getFogStack();
        if (!FogStack::push(stack, id, tag))
            return fogError(ctx, false);
        sendFog(ctx, *player);
        return JS_NewInt32(ctx, (int32_t) stack.size() - 1);
    }

    // a tag pops the topmost entry carrying it, none pops the top
    JSValue fogPop(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        std::string tag;
        bool tagged = false;
        if (!readTag(ctx, argc, argv, 0, tag, tagged))
            return typeError(ctx);
        JSValue error = JS_UNDEFINED;
        ServerPlayer *player = fogOwner(ctx, thisVal, "pop", error);
        if (player == nullptr)
            return error;
        const std::string id = FogStack::pop(player->getFogStack(), tagged ? &tag : nullptr);
        if (id.empty())
            return JS_UNDEFINED;
        sendFog(ctx, *player);
        return string(ctx, id);
    }

    // a tag drops every entry carrying it, none clears the stack
    JSValue fogRemove(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        std::string tag;
        bool tagged = false;
        if (!readTag(ctx, argc, argv, 0, tag, tagged))
            return typeError(ctx);
        JSValue error = JS_UNDEFINED;
        ServerPlayer *player = fogOwner(ctx, thisVal, "remove", error);
        if (player == nullptr)
            return error;
        if (!FogStack::remove(player->getFogStack(), tagged ? &tag : nullptr))
            return JS_FALSE;
        sendFog(ctx, *player);
        return JS_TRUE;
    }

    // the limit first, then every identifier
    JSValue fogSetStack(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        std::string tag;
        bool tagged = false;
        std::vector<std::string> ids;
        bool ok = argc >= 1 && JS_IsArray(argv[0]) && readTag(ctx, argc, argv, 1, tag, tagged);
        if (ok) {
            uint32_t length = 0;
            JSValue lengthValue = JS_GetPropertyStr(ctx, argv[0], "length");
            JS_ToUint32(ctx, &length, lengthValue);
            JS_FreeValue(ctx, lengthValue);
            for (uint32_t i = 0; ok && i < length; ++i) {
                JSValue entry = JS_GetPropertyUint32(ctx, argv[0], i);
                ok = JS_IsString(entry);
                if (ok)
                    ids.push_back(toString(ctx, entry));
                JS_FreeValue(ctx, entry);
            }
        }
        if (!ok)
            return typeError(ctx);
        JSValue error = JS_UNDEFINED;
        ServerPlayer *player = fogOwner(ctx, thisVal, "setStack", error);
        if (player == nullptr)
            return error;
        if (ids.size() > FogStack::LIMIT)
            return fogError(ctx, false);
        if (!std::all_of(ids.begin(), ids.end(), FogStack::isValidId))
            return fogError(ctx, true);
        FogStack::Entries &stack = player->getFogStack();
        stack.clear();
        for (const std::string &id: ids)
            stack.push_back({id, tag});
        sendFog(ctx, *player);
        return JS_UNDEFINED;
    }

    JSValue playerGetFogSettings(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptEntityRef *ref = api->native<ScriptEntityRef>(thisVal);
        if (ref == nullptr)
            return typeError(ctx);
        ScriptFogRef *fog = new ScriptFogRef();
        fog->mEntity = *ref;
        return api->makeInstance("FogSettings", fog);
    }
}

void ScriptApi::_bindFog() {
    if (!mSurface.has("FogSettings"))
        return;

    JSValue fog = defineClass("FogSettings");
    method(fog, "FogSettings", "getStack", fogGetStack, 0);
    method(fog, "FogSettings", "getTags", fogGetTags, 0);
    method(fog, "FogSettings", "push", fogPush, 2);
    method(fog, "FogSettings", "pop", fogPop, 1);
    method(fog, "FogSettings", "remove", fogRemove, 1);
    method(fog, "FogSettings", "setStack", fogSetStack, 2);
    property(prototypeOf("Player"), "Player", "fogSettings", playerGetFogSettings);
}
