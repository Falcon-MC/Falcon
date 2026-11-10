#include "Scripting/Binding/ScriptNatives.h"

namespace ScriptValues {
    std::string toString(JSContext *ctx, JSValueConst value) {
        const char *chars = JS_ToCString(ctx, value);
        if (chars == nullptr)
            return std::string();

        std::string result(chars);
        JS_FreeCString(ctx, chars);
        return result;
    }

    JSValue string(JSContext *ctx, const std::string &value) {
        return JS_NewStringLen(ctx, value.data(), value.size());
    }

    JSValue vector3(JSContext *ctx, float x, float y, float z) {
        JSValue object = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, object, "x", JS_NewFloat64(ctx, x));
        JS_SetPropertyStr(ctx, object, "y", JS_NewFloat64(ctx, y));
        JS_SetPropertyStr(ctx, object, "z", JS_NewFloat64(ctx, z));
        return object;
    }

    bool readVector3(JSContext *ctx, JSValueConst value, double &x, double &y, double &z) {
        if (!JS_IsObject(value))
            return false;

        JSValue xValue = JS_GetPropertyStr(ctx, value, "x");
        JSValue yValue = JS_GetPropertyStr(ctx, value, "y");
        JSValue zValue = JS_GetPropertyStr(ctx, value, "z");

        const bool ok = JS_ToFloat64(ctx, &x, xValue) == 0 && JS_ToFloat64(ctx, &y, yValue) == 0
                        && JS_ToFloat64(ctx, &z, zValue) == 0;

        JS_FreeValue(ctx, xValue);
        JS_FreeValue(ctx, yValue);
        JS_FreeValue(ctx, zValue);
        return ok;
    }

    std::string messageToRawText(JSContext *ctx, JSValueConst value, bool &isJson) {
        if (!JS_IsObject(value)) {
            isJson = false;
            return toString(ctx, value);
        }

        isJson = true;
        JSValue rawtext = JS_GetPropertyStr(ctx, value, "rawtext");
        JSValue target;
        if (JS_IsUndefined(rawtext)) {
            JSValue wrap = JS_NewObject(ctx);
            JSValue array = JS_NewArray(ctx);
            JS_SetPropertyUint32(ctx, array, 0, JS_DupValue(ctx, value));
            JS_SetPropertyStr(ctx, wrap, "rawtext", array);
            target = wrap;
        } else {
            target = JS_DupValue(ctx, value);
        }
        JS_FreeValue(ctx, rawtext);

        JSValue json = JS_JSONStringify(ctx, target, JS_UNDEFINED, JS_UNDEFINED);
        std::string result = toString(ctx, json);
        JS_FreeValue(ctx, json);
        JS_FreeValue(ctx, target);
        return result;
    }

    JSValue dynamicPropertyGet(JSContext *ctx, const std::unordered_map<std::string, DynamicPropertyValue> &store,
                               const std::string &id) {
        const auto it = store.find(id);
        if (it == store.end())
            return JS_UNDEFINED;

        const DynamicPropertyValue &value = it->second;
        switch (value.mType) {
            case DynamicPropertyValue::Type::Boolean:
                return JS_NewBool(ctx, value.mBoolean);
            case DynamicPropertyValue::Type::Number:
                return JS_NewFloat64(ctx, value.mNumber);
            case DynamicPropertyValue::Type::String:
                return string(ctx, value.mString);
            case DynamicPropertyValue::Type::Vector:
                return vector3(ctx, value.mVector[0], value.mVector[1], value.mVector[2]);
            default:
                return JS_UNDEFINED;
        }
    }

    void dynamicPropertySet(JSContext *ctx, std::unordered_map<std::string, DynamicPropertyValue> &store,
                            const std::string &id, JSValueConst value) {
        if (JS_IsUndefined(value) || JS_IsNull(value)) {
            store.erase(id);
            return;
        }

        DynamicPropertyValue stored;
        if (JS_IsBool(value)) {
            stored.mType = DynamicPropertyValue::Type::Boolean;
            stored.mBoolean = JS_ToBool(ctx, value) == 1;
        } else if (JS_IsNumber(value)) {
            stored.mType = DynamicPropertyValue::Type::Number;
            JS_ToFloat64(ctx, &stored.mNumber, value);
        } else if (JS_IsString(value)) {
            stored.mType = DynamicPropertyValue::Type::String;
            stored.mString = toString(ctx, value);
        } else if (JS_IsObject(value)) {
            double x = 0.0, y = 0.0, z = 0.0;
            readVector3(ctx, value, x, y, z);
            stored.mType = DynamicPropertyValue::Type::Vector;
            stored.mVector[0] = (float) x;
            stored.mVector[1] = (float) y;
            stored.mVector[2] = (float) z;
        } else {
            return;
        }

        store[id] = stored;
    }

    JSValue dynamicPropertyIds(JSContext *ctx, const std::unordered_map<std::string, DynamicPropertyValue> &store) {
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const auto &entry: store)
            JS_SetPropertyUint32(ctx, array, index++, string(ctx, entry.first));
        return array;
    }

    bool arrayContainsString(JSContext *ctx, JSValueConst array, const std::string &needle) {
        if (!JS_IsArray(array))
            return false;

        uint32_t length = 0;
        JSValue lengthValue = JS_GetPropertyStr(ctx, array, "length");
        JS_ToUint32(ctx, &length, lengthValue);
        JS_FreeValue(ctx, lengthValue);

        for (uint32_t i = 0; i < length; ++i) {
            JSValue element = JS_GetPropertyUint32(ctx, array, i);
            const std::string value = toString(ctx, element);
            JS_FreeValue(ctx, element);
            if (value == needle)
                return true;
        }
        return false;
    }

    JSValue statesToJs(JSContext *ctx, const Tag &states) {
        JSValue object = JS_NewObject(ctx);
        const std::vector<std::string> &keys = states.getKeys();
        const std::vector<Tag> &values = states.getValues();
        for (size_t index = 0; index < keys.size(); ++index) {
            const Tag &value = values[index];
            JSValue member = JS_UNDEFINED;
            if (value.getType() == Tag::Type::Byte)
                member = JS_NewBool(ctx, value.asByte() != 0);
            else if (value.getType() == Tag::Type::Short)
                member = JS_NewInt32(ctx, value.asShort());
            else if (value.getType() == Tag::Type::Int)
                member = JS_NewInt32(ctx, value.asInt());
            else if (value.getType() == Tag::Type::String)
                member = string(ctx, value.asString());
            JS_SetPropertyStr(ctx, object, keys[index].c_str(), member);
        }
        return object;
    }

    Tag statesFromJs(JSContext *ctx, JSValueConst object) {
        Tag states = Tag::ofCompound();
        JSPropertyEnum *properties = nullptr;
        uint32_t count = 0;
        if (!JS_IsObject(object)
            || JS_GetOwnPropertyNames(ctx, &properties, &count, object, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) != 0)
            return states;

        for (uint32_t index = 0; index < count; ++index) {
            const char *name = JS_AtomToCString(ctx, properties[index].atom);
            JSValue value = JS_GetProperty(ctx, object, properties[index].atom);
            if (name != nullptr) {
                if (JS_IsBool(value))
                    states.putByte(name, JS_ToBool(ctx, value) == 1 ? 1 : 0);
                else if (JS_IsNumber(value)) {
                    int32_t number = 0;
                    JS_ToInt32(ctx, &number, value);
                    states.putInt(name, number);
                } else if (JS_IsString(value))
                    states.putString(name, toString(ctx, value));
                JS_FreeCString(ctx, name);
            }
            JS_FreeValue(ctx, value);
            JS_FreeAtom(ctx, properties[index].atom);
        }
        js_free(ctx, properties);
        return states;
    }

    bool dynamicPropertySetAll(JSContext *ctx, std::unordered_map<std::string, DynamicPropertyValue> &store,
                               JSValueConst values) {
        JSPropertyEnum *properties = nullptr;
        uint32_t count = 0;
        if (JS_GetOwnPropertyNames(ctx, &properties, &count, values, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) != 0)
            return false;
        for (uint32_t i = 0; i < count; ++i) {
            JSValue key = JS_AtomToString(ctx, properties[i].atom);
            JSValue value = JS_GetProperty(ctx, values, properties[i].atom);
            dynamicPropertySet(ctx, store, toString(ctx, key), value);
            JS_FreeValue(ctx, value);
            JS_FreeValue(ctx, key);
        }
        JS_FreePropertyEnum(ctx, properties, count);
        return true;
    }

    // TODO: the game's own accounting is unverified; key and value payload bytes for now
    int64_t dynamicPropertyByteCount(const std::unordered_map<std::string, DynamicPropertyValue> &store) {
        int64_t total = 0;
        for (const auto &entry: store) {
            total += (int64_t) entry.first.size();
            switch (entry.second.mType) {
                case DynamicPropertyValue::Type::Boolean:
                    total += 1;
                    break;
                case DynamicPropertyValue::Type::Number:
                    total += 8;
                    break;
                case DynamicPropertyValue::Type::String:
                    total += (int64_t) entry.second.mString.size();
                    break;
                case DynamicPropertyValue::Type::Vector:
                    total += 12;
                    break;
                default:
                    break;
            }
        }
        return total;
    }
}
