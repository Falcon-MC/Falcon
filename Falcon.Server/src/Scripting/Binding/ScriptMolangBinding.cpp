#include "Scripting/Binding/ScriptApi.h"

#include <cstdio>

using namespace ScriptValues;

namespace {
    JSValue typeError(JSContext *ctx) {
        return JS_ThrowTypeError(ctx, "Native type conversion failed.");
    }

    ScriptMolangMap *mapOf(JSContext *ctx, JSValueConst thisVal) {
        return ScriptApi::fromContext(ctx)->native<ScriptMolangMap>(thisVal);
    }

    bool readFloat(JSContext *ctx, JSValueConst value, float &out) {
        double number = 0.0;
        if (!JS_IsNumber(value) || JS_ToFloat64(ctx, &number, value) != 0)
            return false;
        out = (float) number;
        return true;
    }

    bool readField(JSContext *ctx, JSValueConst object, const char *key, float &out) {
        JSValue value = JS_GetPropertyStr(ctx, object, key);
        const bool read = readFloat(ctx, value, out);
        JS_FreeValue(ctx, value);
        return read;
    }

    // ScriptMolangVariableMap name check: "variable." kept, a short "v." kept, any other
    // dot refused, a bare name prefixed
    bool normalizeName(JSContext *ctx, JSValueConst value, std::string &name) {
        if (!JS_IsString(value)) {
            typeError(ctx);
            return false;
        }
        name = toString(ctx, value);
        if (name.size() > 8) {
            if (name.compare(0, 9, "variable.") == 0)
                return true;
        } else if (name.compare(0, 2, "v.") == 0) {
            return true;
        }
        if (name.find('.') != std::string::npos) {
            JSValue error = JS_NewError(ctx);
            JS_SetPropertyStr(
                    ctx, error, "message",
                    JS_NewString(ctx, "Variable name cannot contain '.' unless it is preceded by 'variable'."));
            JS_Throw(ctx, error);
            return false;
        }
        name = "variable." + name;
        return true;
    }

    void store(ScriptMolangMap &map, ScriptMolangMap::Variable variable) {
        for (ScriptMolangMap::Variable &existing: map.mVariables) {
            if (existing.mName == variable.mName) {
                existing = std::move(variable);
                return;
            }
        }
        map.mVariables.push_back(std::move(variable));
    }

    JSValue molangConstructor(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return ScriptApi::fromContext(ctx)->makeInstance("MolangVariableMap", new ScriptMolangMap());
    }

    JSValue molangSetFloat(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptMolangMap *map = mapOf(ctx, thisVal);
        ScriptMolangMap::Variable variable;
        if (map == nullptr || argc < 2)
            return typeError(ctx);
        if (!normalizeName(ctx, argv[0], variable.mName))
            return JS_EXCEPTION;
        if (!readFloat(ctx, argv[1], variable.mValue))
            return typeError(ctx);
        store(*map, std::move(variable));
        return JS_UNDEFINED;
    }

    // magic 0: setColorRGB, 1: setColorRGBA
    JSValue molangSetColor(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic) {
        ScriptMolangMap *map = mapOf(ctx, thisVal);
        ScriptMolangMap::Variable variable;
        if (map == nullptr || argc < 2)
            return typeError(ctx);
        if (!normalizeName(ctx, argv[0], variable.mName))
            return JS_EXCEPTION;
        float red = 0.0f, green = 0.0f, blue = 0.0f, alpha = 0.0f;
        if (!JS_IsObject(argv[1]) || !readField(ctx, argv[1], "red", red) || !readField(ctx, argv[1], "green", green)
            || !readField(ctx, argv[1], "blue", blue) || (magic == 1 && !readField(ctx, argv[1], "alpha", alpha)))
            return typeError(ctx);
        variable.mArray = true;
        variable.mMembers = {{".r", red}, {".g", green}, {".b", blue}};
        if (magic == 1)
            variable.mMembers.emplace_back(".a", alpha);
        store(*map, std::move(variable));
        return JS_UNDEFINED;
    }

    JSValue molangSetVector3(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptMolangMap *map = mapOf(ctx, thisVal);
        ScriptMolangMap::Variable variable;
        if (map == nullptr || argc < 2)
            return typeError(ctx);
        if (!normalizeName(ctx, argv[0], variable.mName))
            return JS_EXCEPTION;
        double x = 0.0, y = 0.0, z = 0.0;
        if (!readVector3(ctx, argv[1], x, y, z))
            return typeError(ctx);
        variable.mArray = true;
        variable.mMembers = {{".x", (float) x}, {".y", (float) y}, {".z", (float) z}};
        store(*map, std::move(variable));
        return JS_UNDEFINED;
    }

    // members in the order the game builds them
    JSValue molangSetSpeedAndDirection(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptMolangMap *map = mapOf(ctx, thisVal);
        ScriptMolangMap::Variable variable;
        if (map == nullptr || argc < 3)
            return typeError(ctx);
        if (!normalizeName(ctx, argv[0], variable.mName))
            return JS_EXCEPTION;
        float speed = 0.0f;
        double x = 0.0, y = 0.0, z = 0.0;
        if (!readFloat(ctx, argv[1], speed) || !readVector3(ctx, argv[2], x, y, z))
            return typeError(ctx);
        variable.mArray = true;
        variable.mMembers = {{".speed", speed},
                             {".direction_x", (float) x},
                             {".direction_y", (float) y},
                             {".direction_z", (float) z}};
        store(*map, std::move(variable));
        return JS_UNDEFINED;
    }

    void appendString(std::string &out, const std::string &text) {
        out += '"';
        for (char c: text) {
            if (c == '"' || c == '\\') {
                out += '\\';
                out += c;
            } else if ((unsigned char) c < 0x20) {
                char escaped[8];
                std::snprintf(escaped, sizeof(escaped), "\\u%04x", (unsigned char) c);
                out += escaped;
            } else {
                out += c;
            }
        }
        out += '"';
    }

    void appendFloat(std::string &out, float value) {
        char text[32];
        std::snprintf(text, sizeof(text), "%.17g", (double) value);
        out += "{\"type\":\"float\",\"value\":";
        out += text;
        out += '}';
    }
}

// MolangVariableMap as the client reads it: an array of {name, value},
// a value either a float or a member_array of named floats
std::string ScriptMolangMap::toJson() const {
    std::string out = "[";
    for (size_t index = 0; index < mVariables.size(); ++index) {
        const Variable &variable = mVariables[index];
        if (index > 0)
            out += ',';
        out += "{\"name\":";
        appendString(out, variable.mName);
        out += ",\"value\":";
        if (!variable.mArray) {
            appendFloat(out, variable.mValue);
        } else {
            out += "{\"type\":\"member_array\",\"value\":[";
            for (size_t member = 0; member < variable.mMembers.size(); ++member) {
                if (member > 0)
                    out += ',';
                out += "{\"name\":";
                appendString(out, variable.mMembers[member].first);
                out += ",\"value\":";
                appendFloat(out, variable.mMembers[member].second);
                out += '}';
            }
            out += "]}";
        }
        out += '}';
    }
    return out + "]";
}

void ScriptApi::_bindMolang() {
    if (!mSurface.has("MolangVariableMap"))
        return;

    JSValue map = defineClass("MolangVariableMap", molangConstructor, 0);
    method(map, "MolangVariableMap", "setFloat", molangSetFloat, 2);
    methodMagic(map, "MolangVariableMap", "setColorRGB", molangSetColor, 2, 0);
    methodMagic(map, "MolangVariableMap", "setColorRGBA", molangSetColor, 2, 1);
    method(map, "MolangVariableMap", "setVector3", molangSetVector3, 2);
    method(map, "MolangVariableMap", "setSpeedAndDirection", molangSetSpeedAndDirection, 3);
}

bool ScriptApi::readMolang(JSValueConst value, std::string &json, bool &present) {
    present = false;
    if (JS_IsUndefined(value))
        return true;
    const ScriptMolangMap *map = native<ScriptMolangMap>(value);
    if (map == nullptr)
        return false;
    json = map->toJson();
    present = true;
    return true;
}
