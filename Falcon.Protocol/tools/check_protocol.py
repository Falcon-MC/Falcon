import argparse
import json
import re
import sys
from pathlib import Path

PUT_TOKENS = {
    "put": ["raw"],
    "putByte": ["u8"],
    "putBool": ["bool"],
    "putOptionalPresent": ["bool"],
    "putShort": ["i16be"],
    "putLShort": ["i16le"],
    "putInt": ["i32be"],
    "putLInt": ["i32le"],
    "putLong": ["i64be"],
    "putLLong": ["i64le"],
    "putFloat": ["f32be"],
    "putLFloat": ["f32le"],
    "putDouble": ["f64be"],
    "putLDouble": ["f64le"],
    "putUnsignedVarInt": ["uvar32"],
    "putArrayLength": ["uvar32"],
    "putVarInt": ["var32"],
    "putUnsignedVarLong": ["uvar64"],
    "putVarLong": ["var64"],
    "putString": ["string"],
    "putByteArray": ["string"],
    "putVector3f": ["f32le", "f32le", "f32le"],
    "putVector2f": ["f32le", "f32le"],
    "putVector3i": ["var32", "var32", "var32"],
    "putBlockPosition": ["var32", "var32", "var32"],
    "putUuid": ["i64le", "i64le"],
}

COMPATIBLE = {("bool", "u8"), ("u8", "bool")}
INTEGERS = {"u8", "cbyte", "i16le", "i16be", "i32le", "i32be", "i64le", "i64be", "var32", "uvar32", "var64", "uvar64"}
OPEN = "["
CLOSE = "]"
OPAQUE = "opaque"
STOP = "stop"
CHOICE = "choice"
CONTROL = "ctl"
NBT = "nbt"
BYTE_ARRAYS = [[[], [OPEN, "u8", CLOSE]], [[], [OPEN, "cbyte", CLOSE]]]
ACCEPT = -1
VERIFIED, OPTIONAL, BYTES, HELPERS = range(4)
EMPTY_INFO = (frozenset(), frozenset(), frozenset(), frozenset())

STATEMENT = re.compile(
    r"\b(for|while|if|switch|else)\b"
    r"|\bstream\s*\.\s*(put\w*)\s*\("
    r"|\b([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)\s*\(\s*stream\b"
    r"|([A-Za-z_][\w.\->\[\]]*?)\s*(?:\.|->)\s*(write\w*)\s*\(\s*stream\b"
)
DEFINITION = re.compile(r"^[\w:<>,\s&*]*?\b([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)\s*\(([^;{]*)\)\s*(?:const\s*)?\{",
                        re.MULTILINE)
CASE_LABEL = re.compile(r"\b(?:case\b[^;{}]*?(?<!:):(?!:)|default\s*:)")
RETURN = re.compile(r"\breturn\b")
THROW = re.compile(r"\bthrow\b")
KEYWORDS = {"if", "for", "while", "switch", "return", "const", "case", "else", "new", "delete", "throw", "sizeof",
            "typename", "struct", "class", "using", "static_cast", "reinterpret_cast"}
WRAPPERS = re.compile(r"(?:std::)?(?:optional|vector|unique_ptr|shared_ptr|list|deque|array|set)\s*<\s*(.+?)\s*"
                      r"(?:,[^<>]*)?>")


class Token:
    def __init__(self, kind, field="", depth=0, detail="", alternatives=None):
        self.kind = kind
        self.field = field
        self.depth = depth
        self.detail = detail
        self.alternatives = alternatives or []
        self.starts = []

    def label(self):
        if self.kind == OPAQUE:
            return f"call {self.detail}"
        if self.kind == STOP:
            return f"unchecked ({self.detail})"
        if self.kind == CHOICE:
            return f"one of {len(self.alternatives)} variants"
        if self.kind == CONTROL:
            return "variant index"
        return self.kind


def signature(tokens):
    return tuple((token.kind, token.detail, tuple(signature(alternative) for alternative in token.alternatives))
                 for token in tokens)


def choice(alternatives, field="", depth=0):
    unique = []
    seen = set()
    for alternative in alternatives:
        key = signature(alternative)
        if key not in seen:
            seen.add(key)
            unique.append(alternative)
    if not unique:
        return []
    if len(unique) == 1:
        return unique[0]
    return [Token(CHOICE, field, depth, alternatives=unique)]


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", text)


def matching(text, start, opening, closing):
    depth = 0
    index = start
    quote = None
    while index < len(text):
        character = text[index]
        if quote:
            if character == "\\":
                index += 2
                continue
            if character == quote:
                quote = None
        elif character in "\"'":
            quote = character
        elif character == opening:
            depth += 1
        elif character == closing:
            depth -= 1
            if depth == 0:
                return index
        index += 1
    return len(text) - 1


def statement_end(text, start):
    index = start
    depth = 0
    while index < len(text):
        character = text[index]
        if character in "({[":
            depth += 1
        elif character in ")}]":
            depth -= 1
        elif character == ";" and depth == 0:
            return index
        index += 1
    return len(text) - 1


def body_after(text, start):
    index = start
    while index < len(text) and text[index].isspace():
        index += 1
    if index < len(text) and text[index] == "{":
        end = matching(text, index, "{", "}")
        return text[index + 1:end], end + 1
    end = statement_end(text, index)
    return text[index:end + 1], end + 1


def split_arguments(text):
    text = text.replace("->", ".")
    parts = []
    depth = 0
    start = 0
    for position, character in enumerate(text):
        if character in "(<[{":
            depth += 1
        elif character in ")>]}":
            depth -= 1
        elif character == "," and depth == 0:
            parts.append(text[start:position])
            start = position + 1
    parts.append(text[start:])
    return [part.strip() for part in parts if part.strip()]


def switch_cases(body):
    labels = []
    for match in CASE_LABEL.finditer(body):
        prefix = body[:match.start()]
        if prefix.count("{") == prefix.count("}"):
            labels.append(match)
    cases = []
    for number, label in enumerate(labels):
        end = labels[number + 1].start() if number + 1 < len(labels) else len(body)
        text = body[label.end():end]
        if text.strip():
            cases.append(text)
    return cases


def unwrap(kind):
    kind = re.sub(r"\bconst\b", "", kind).strip()
    while True:
        match = WRAPPERS.fullmatch(kind)
        if not match:
            return kind
        kind = match.group(1).strip()


class Scope:
    def __init__(self, path, text, bindings=None, calls=0):
        self.path = path
        self.text = text
        self.bindings = bindings or {}
        self.calls = calls


class FalconIndex:
    def __init__(self, root):
        self.root = root
        self.functions = {}
        self.local = {}
        self.free = {}
        self.headers = {}
        for path in sorted((root / "include").rglob("*.h")):
            self.headers[path] = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
        for path in sorted((root / "src").rglob("*.cpp")):
            text = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
            self.local[path] = {}
            for match in DEFINITION.finditer(text):
                name = match.group(1)
                if "stream" not in match.group(2) or name.split("::")[-1] in KEYWORDS:
                    continue
                open_brace = text.index("{", match.end() - 1)
                body = text[open_brace + 1:matching(text, open_brace, "{", "}")]
                entry = (path, match.group(2), body)
                self.local[path][name.split("::")[-1]] = entry
                if "::" in name:
                    self.functions[name] = entry
                else:
                    self.free.setdefault(name, []).append(entry)

    def resolve(self, path, name):
        if name in self.functions:
            return self.functions[name]
        short = name.split("::")[-1]
        if "::" not in name and short in self.local.get(path, {}):
            return self.local[path][short]
        if "::" not in name and len(self.free.get(name, [])) == 1:
            return self.free[name][0]
        return None

    def method(self, kind, name):
        short = kind.split("::")[-1]
        for qualified, entry in self.functions.items():
            parts = qualified.split("::")
            if len(parts) >= 2 and parts[-2] == short and parts[-1] == name:
                return entry
        return None

    def own_header(self, path):
        try:
            relative = path.relative_to(self.root / "src")
        except ValueError:
            return None
        return self.headers.get(self.root / "include" / relative.with_suffix(".h"))

    def declared_types(self, name, scope, depth=0):
        pattern = re.compile(r"([A-Za-z_][\w:]*(?:\s*<[^;(){}]*>)?)\s*[&*]*\s*\b" + re.escape(name)
                             + r"\b\s*(?=[;=,){}:\[]|$)", re.MULTILINE)
        texts = [scope.text]
        header = self.own_header(scope.path)
        if header:
            texts.append(header)
        texts.extend(self.headers.values())
        found = []
        for text in texts:
            for match in pattern.finditer(text):
                kind = unwrap(match.group(1))
                if kind in KEYWORDS or kind in found:
                    continue
                if kind == "auto":
                    if depth < 3:
                        container = re.search(r"\b" + re.escape(name) + r"\s*:\s*([^)]+)\)", scope.text)
                        if container:
                            identifiers = re.findall(r"[A-Za-z_]\w*", container.group(1))
                            if identifiers:
                                found.extend(self.declared_types(identifiers[-1], scope, depth + 1))
                    continue
                found.append(kind)
        return found

    def member(self, expression, method, scope):
        identifiers = re.findall(r"[A-Za-z_]\w*", re.sub(r"\[[^\]]*\]", "", expression))
        if not identifiers:
            return None
        for kind in self.declared_types(identifiers[-1], scope):
            entry = self.method(kind, method)
            if entry:
                return entry
        return None


def enter(entry, arguments, scope):
    path, parameters, body = entry
    bindings = {}
    names = []
    for parameter in split_arguments(parameters):
        identifiers = re.findall(r"[A-Za-z_]\w*", parameter.split("=")[0])
        names.append(identifiers[-1] if identifiers else "")
    for name, argument in zip(names, split_arguments(arguments)):
        argument = argument.lstrip("&*").strip()
        if re.fullmatch(r"[A-Za-z_][\w:]*", argument):
            bindings[name] = scope.bindings.get(argument, argument)
    return Scope(path, parameters + "\n" + body, bindings, scope.calls + 1)


def falcon_tokens(index, scope, code, depth=0):
    tokens = []
    position = 0
    while True:
        match = STATEMENT.search(code, position)
        if not match:
            return tokens

        keyword, put, call, member, member_call = match.groups()
        if keyword in ("for", "while"):
            header_start = code.index("(", match.end())
            header_end = matching(code, header_start, "(", ")")
            body, position = body_after(code, header_end + 1)
            inner = falcon_tokens(index, scope, body, depth + 1)
            if inner:
                tokens.append(Token(OPEN, depth=depth))
                tokens.extend(inner)
                tokens.append(Token(CLOSE, depth=depth))
        elif keyword == "if":
            header_start = code.index("(", match.end())
            header_end = matching(code, header_start, "(", ")")
            body, position = body_after(code, header_end + 1)
            branches = [body]
            complete = False
            while True:
                rest = re.match(r"\s*else\b", code[position:])
                if not rest:
                    break
                position += rest.end()
                chained = re.match(r"\s*if\s*\(", code[position:])
                if chained:
                    header_start = position + chained.end() - 1
                    header_end = matching(code, header_start, "(", ")")
                    body, position = body_after(code, header_end + 1)
                else:
                    body, position = body_after(code, position)
                    complete = True
                branches.append(body)
            live = [body for body in branches if not THROW.search(body)]
            if any(RETURN.search(body) for body in live):
                rest = falcon_tokens(index, scope, code[position:], depth)
                alternatives = []
                for body in live:
                    inner = falcon_tokens(index, scope, body, depth)
                    alternatives.append(inner if RETURN.search(body) else inner + rest)
                if not complete:
                    alternatives.append(rest)
                tokens.extend(choice(alternatives, depth=depth))
                return tokens
            alternatives = [falcon_tokens(index, scope, body, depth) for body in live]
            tokens.extend(choice(alternatives, depth=depth))
        elif keyword == "switch":
            header_start = code.index("(", match.end())
            header_end = matching(code, header_start, "(", ")")
            body, position = body_after(code, header_end + 1)
            cases = [text for text in switch_cases(body) if not THROW.search(text)]
            tokens.extend(choice([falcon_tokens(index, scope, text, depth) for text in cases], depth=depth))
        elif keyword == "else":
            _, position = body_after(code, match.end())
        elif put:
            open_paren = match.end() - 1
            position = matching(code, open_paren, "(", ")") + 1
            for kind in PUT_TOKENS.get(put, []):
                tokens.append(Token(kind, depth=depth))
            if put not in PUT_TOKENS:
                tokens.append(Token(OPAQUE, depth=depth, detail=f"stream.{put}"))
        elif call:
            open_paren = code.index("(", match.start())
            close_paren = matching(code, open_paren, "(", ")")
            position = close_paren + 1
            name = scope.bindings.get(call, call)
            if name.startswith("NbtIo::write"):
                tokens.append(Token(NBT, depth=depth))
                continue
            resolved = index.resolve(scope.path, name) if scope.calls < 12 else None
            if resolved:
                inner = enter(resolved, code[open_paren + 1:close_paren], scope)
                tokens.extend(falcon_tokens(index, inner, resolved[2], depth))
            else:
                tokens.append(Token(OPAQUE, depth=depth, detail=name))
        else:
            open_paren = code.index("(", match.start() + len(member))
            close_paren = matching(code, open_paren, "(", ")")
            position = close_paren + 1
            resolved = index.member(member, member_call, scope) if scope.calls < 12 else None
            if resolved:
                inner = enter(resolved, code[open_paren + 1:close_paren], scope)
                tokens.extend(falcon_tokens(index, inner, resolved[2], depth))
            else:
                tokens.append(Token(OPAQUE, depth=depth, detail=f"{member}.{member_call}"))


class MojangDocs:
    def __init__(self, root):
        self.root = root
        self.cache = {}

    def load(self, name):
        if name not in self.cache:
            path = self.root / name
            self.cache[name] = json.loads(path.read_text(encoding="utf-8")) if path.exists() else None
        return self.cache[name]

    def packets(self):
        result = {}
        for path in sorted(self.root.glob("*Packet.json")):
            schema = self.load(path.name)
            meta = (schema or {}).get("$metaProperties", {})
            if "[cereal:packet]" in meta:
                result[schema["title"]] = (meta["[cereal:packet]"], schema)
        return result

    def protocol(self):
        for schema in self.packets().values():
            return schema[1].get("x-protocol-version")
        return None


def integer_token(underlying, options):
    width = {"int8": 8, "uint8": 8, "int16": 16, "uint16": 16, "int32": 32, "uint32": 32,
             "int64": 64, "uint64": 64}.get(underlying)
    if width is None:
        return None
    if width == 8:
        return "cbyte" if underlying == "uint8" and "Compression" in options else "u8"
    if "Compression" in options:
        prefix = "var" if underlying.startswith("int") else "uvar"
        return f"{prefix}{64 if width == 64 else 32}"
    return f"i{width}{'be' if 'Big Endian' in options else 'le'}"


def primitive_token(schema):
    underlying = schema.get("x-underlying-type")
    options = schema.get("x-serialization-options", [])
    if "enum" in schema:
        if "Enum-as-Value" not in options:
            return "string"
        return integer_token(underlying, options) if underlying else None
    if underlying == "bool" or schema.get("type") == "boolean":
        return "bool"
    if underlying in ("float", "double"):
        return f"f{32 if underlying == 'float' else 64}{'be' if 'Big Endian' in options else 'le'}"
    if underlying:
        return integer_token(underlying, options)
    if schema.get("type") == "integer":
        return "var32" if "Compression" in options else "i32le"
    if schema.get("type") == "string" and "enum" not in schema:
        return "string"
    return None


def map_tokens(docs, schema, field, depth, seen):
    value = schema["additionalProperties"]
    pair = isinstance(value, dict) and value.get("type") == "object"
    if pair and set(value.get("properties", {})) == {"key", "value"}:
        key = mojang_tokens(docs, value["properties"]["key"], f"{field}.key", depth + 1, seen)
        entry = mojang_tokens(docs, value["properties"]["value"], f"{field}.value", depth + 1, seen)
    else:
        key = mojang_tokens(docs, schema.get("propertyNames", {"type": "string"}), f"{field}.key", depth + 1, seen)
        entry = mojang_tokens(docs, value if isinstance(value, dict) else {}, f"{field}.value", depth + 1, seen)
    return [Token("uvar32", field, depth), Token(OPEN, field, depth)] + key + entry + [Token(CLOSE, field, depth)]


def mojang_tokens(docs, schema, field, depth, seen):
    if schema is None:
        return [Token(STOP, field, depth, "missing schema")]

    if "oneOf" in schema or "anyOf" in schema:
        options = schema.get("oneOf", schema.get("anyOf"))
        alternatives = [mojang_tokens(docs, option, field, depth, seen) for option in options]
        tokens = [Token(CONTROL, field, depth)] if "x-control-value-type" in schema else []
        return tokens + choice(alternatives, field, depth)

    if "$ref" not in schema:
        token = primitive_token(schema)
        if token:
            return [Token(token, field, depth)]

    if "$ref" in schema:
        name = schema["$ref"].split("/")[-1]
        target = docs.load(name)
        if target is not None and target.get("$ref", "").split("/")[-1] == name:
            return [Token("string", field, depth)]
        if name in seen:
            return [Token(STOP, field, depth, f"recursive {name}")]
        if target is not None:
            target = dict(target)
            for key in ("x-underlying-type", "x-serialization-options"):
                if key in schema and key not in target:
                    target[key] = schema[key]
        return mojang_tokens(docs, target, field, depth, seen | {name})

    kind = schema.get("type")
    if kind == "array":
        options = schema.get("x-serialization-options", [])
        if "No size compression" in options:
            length = "i32be" if "Big Endian" in options else "i32le"
        else:
            length = "uvar32"
        item_schema = dict(schema.get("items", {}))
        inherited = [option for option in options if option != "No size compression"]
        if inherited and "x-serialization-options" not in item_schema:
            item_schema["x-serialization-options"] = inherited
        items = mojang_tokens(docs, item_schema, field, depth + 1, seen)
        body = [Token(OPEN, field, depth)] + items + [Token(CLOSE, field, depth)]
        if "minItems" in schema and schema.get("minItems") == schema.get("maxItems"):
            count = int(schema["minItems"])
            return choice([body, items * count], field, depth) if count <= 8 else body
        return [Token(length, field, depth)] + choice([body, []], field, depth)

    if kind == "object":
        if "additionalProperties" in schema and not schema.get("properties"):
            return map_tokens(docs, schema, field, depth, seen)
        properties = schema.get("properties", {})
        required = set(schema.get("required", properties.keys()))
        ordered = sorted(properties.items(), key=lambda item: item[1].get("x-ordinal-index", 0))
        tokens = []
        for name, child in ordered:
            path = f"{field}.{name}" if field else name
            start = len(tokens)
            child_tokens = mojang_tokens(docs, child, path, depth, seen)
            if name not in required and "default" not in child:
                tokens.append(Token("bool", path, depth, "optional presence"))
                tokens.extend(choice([child_tokens, []], path, depth))
            else:
                tokens.extend(child_tokens)
            if start < len(tokens):
                tokens[start].starts.append(path)
        return tokens

    if kind == "null":
        return []

    if kind is None:
        return [Token(NBT, field, depth)]

    return [Token(STOP, field, depth, f"unsupported {kind}")]


def top_field(token):
    return token.field.split(".")[0] if token.field else ""


def same(want, have):
    if have.kind == CHOICE:
        return True
    if want.kind == "cbyte":
        return have.kind in ("u8", "uvar32", "bool")
    if want.kind == CONTROL:
        return have.kind in INTEGERS or have.kind == "bool"
    if want.kind == NBT:
        return have.kind in (NBT, "raw")
    return want.kind == have.kind or (want.kind, have.kind) in COMPATIBLE


class Node:
    def __init__(self, token):
        self.token = token
        self.following = frozenset()


class Pattern:
    def __init__(self, tokens):
        self.nodes = []
        self.start = self.build(tokens, frozenset([ACCEPT]))

    def build(self, tokens, following):
        entry = following
        for token in reversed(tokens):
            if token.kind == CHOICE:
                merged = set()
                for alternative in token.alternatives:
                    if alternative:
                        alternative[0].starts += [path for path in token.starts if path not in alternative[0].starts]
                    merged |= self.build(alternative, entry)
                entry = frozenset(merged)
                continue
            node = Node(token)
            self.nodes.append(node)
            number = len(self.nodes) - 1
            node.following = frozenset([number]) if token.kind == STOP else entry
            entry = frozenset([number])
        return entry

    def skip_field(self, number, depth):
        field = top_field(self.nodes[number].token)
        stops = set()
        pending = list(self.nodes[number].following)
        visited = set()
        while pending:
            current = pending.pop()
            if current in visited:
                continue
            visited.add(current)
            if current == ACCEPT:
                stops.add(current)
                continue
            token = self.nodes[current].token
            ends = token.kind == CLOSE and token.depth < depth
            leaves = token.depth <= depth and token.kind != CLOSE and top_field(token) != field
            if ends or leaves or token.kind == STOP:
                stops.add(current)
                continue
            pending.extend(self.nodes[current].following)
        return stops

    def skip_exact(self, number, field):
        stops = set()
        pending = [number]
        visited = set()
        while pending:
            current = pending.pop()
            if current in visited:
                continue
            visited.add(current)
            if current == ACCEPT:
                stops.add(current)
                continue
            token = self.nodes[current].token
            if token.field != field and not token.field.startswith(field + "."):
                stops.add(current)
                continue
            pending.extend(self.nodes[current].following)
        return stops


def noted(info, slot, value):
    parts = list(info)
    parts[slot] = parts[slot] | {value}
    return tuple(parts)


def add_state(states, number, info):
    if number in states:
        current = states[number]
        merged = [first | second for first, second in zip(current, info)]
        merged[OPTIONAL] = min(current[OPTIONAL], info[OPTIONAL], key=len)
        info = tuple(merged)
    states[number] = info


def advance(pattern, states, have, lookahead):
    result = {}
    for number, info in states.items():
        if number == ACCEPT:
            continue
        want = pattern.nodes[number].token
        if want.kind == STOP:
            add_state(result, number, info)
            continue
        if have.kind == OPAQUE:
            for target in pattern.skip_field(number, have.depth):
                add_state(result, target, noted(info, HELPERS, have.detail))
            continue
        if have.kind == "bool" and want.starts and want.kind not in (OPEN, CLOSE) \
                and want.detail != "optional presence":
            add_state(result, number, noted(info, OPTIONAL, f"`{want.field}`"))
            for path in want.starts:
                for target in pattern.skip_exact(number, path):
                    add_state(result, target, noted(info, OPTIONAL, f"`{path}`"))
        if same(want, have):
            if want.kind not in (OPEN, CLOSE):
                info = noted(info, VERIFIED, top_field(want))
            if want.kind == "cbyte":
                info = noted(info, BYTES, f"`{want.field}` as {have.kind}")
            for target in pattern.nodes[number].following:
                add_state(result, target, info)
    return result


def expectations(pattern, states):
    labels = []
    for number in sorted(states, reverse=True):
        if number == ACCEPT:
            continue
        token = pattern.nodes[number].token
        entry = (token.field, token.label())
        if entry not in labels:
            labels.append(entry)
    return labels


def describe(pattern, states, have):
    labels = expectations(pattern, states)
    if not labels:
        return f"Falcon writes an extra {have.label()} after the last field"
    wanted = " or ".join(dict.fromkeys(label for _, label in labels[:4]))
    return f"`{labels[0][0] or '?'}`: expected {wanted}, Falcon writes {have.label()}"


def run(pattern, tokens, states, lookahead, trace, indent=0):
    for position, have in enumerate(tokens):
        following = tokens[position + 1] if position + 1 < len(tokens) else lookahead
        if have.kind == CHOICE:
            outcomes = []
            for number, alternative in enumerate(have.alternatives):
                if trace is not None:
                    trace.append(f"{'  ' * indent}variant {number + 1} of {len(have.alternatives)}")
                outcome = run(pattern, alternative, states, following, trace, indent + 1)
                if isinstance(outcome, str):
                    return outcome
                outcomes.append(outcome)
            common = set(outcomes[0]).intersection(*outcomes[1:])
            if not common:
                return f"the {len(outcomes)} variants Falcon writes after `{describe(pattern, states, have)}` " \
                       f"end at different fields"
            merged = {}
            for outcome in outcomes:
                for number in common:
                    add_state(merged, number, outcome[number])
            states = merged
            continue
        if trace is not None:
            wanted = ", ".join(f"{label} {field}" for field, label in expectations(pattern, states)[:3])
            trace.append(f"{'  ' * indent}{have.label():14} | {wanted[:100]}")
        advanced = advance(pattern, states, have, following)
        if not advanced:
            return describe(pattern, states, have)
        states = advanced
    return states


def check(expected, actual, trace=None):
    pattern = Pattern(expected)
    outcome = run(pattern, actual, {number: EMPTY_INFO for number in pattern.start}, None, trace)
    if isinstance(outcome, str):
        return "mismatch", outcome, EMPTY_INFO
    if ACCEPT in outcome:
        return "match", "", outcome[ACCEPT]
    for number in sorted(outcome, reverse=True):
        token = pattern.nodes[number].token
        if token.kind == STOP:
            return "partial", f"stopped at `{token.field}`: {token.detail}", outcome[number]
    field, label = expectations(pattern, outcome)[0]
    return "mismatch", f"`{field}` ({label}) is never written", EMPTY_INFO


def packet_ids(header):
    text = strip_comments(header.read_text(encoding="utf-8"))
    return {name: int(value) for name, value in re.findall(r"\b(\w+)\s*=\s*(\d+)\s*,", text)}


def falcon_protocol(root):
    info = root / "include" / "Protocol" / "ProtocolInfo.h"
    match = re.search(r"CURRENT_PROTOCOL\s*=\s*(\d+)", info.read_text(encoding="utf-8"))
    return int(match.group(1)) if match else None


def as_string(head):
    blob = Token("string", head.field, head.depth)
    blob.starts = head.starts
    return blob


def merge_length_prefixed(tokens):
    merged = []
    for token in tokens:
        if token.kind == CHOICE:
            token.alternatives = [merge_length_prefixed(alternative) for alternative in token.alternatives]
            shapes = sorted([entry.kind for entry in alternative] for alternative in token.alternatives)
            if merged and merged[-1].kind == "uvar32" and shapes in BYTE_ARRAYS:
                merged[-1] = as_string(merged[-1])
                continue
        if token.kind == "raw" and merged and merged[-1].kind == "uvar32":
            merged[-1] = as_string(merged[-1])
            continue
        merged.append(token)
        kinds = [entry.kind for entry in merged[-4:]]
        if len(kinds) == 4 and kinds[:2] == ["uvar32", OPEN] and kinds[2] in ("u8", "cbyte") and kinds[3] == CLOSE:
            blob = as_string(merged[-4])
            del merged[-4:]
            merged.append(blob)
    return merged


def falcon_packets(root, index):
    packets = {}
    for path in sorted((root / "src" / "Protocol" / "Packets").glob("*.cpp")):
        text = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
        match = re.search(r"void\s+(\w+)::write\s*\(\s*BinaryStream\s*&\s*stream", text)
        if not match:
            continue
        open_brace = text.index("{", match.end())
        body = text[open_brace + 1:matching(text, open_brace, "{", "}")]
        scope = Scope(path, body)
        packets[match.group(1)] = merge_length_prefixed(falcon_tokens(index, scope, body))
    return packets


def explain(expected, actual):
    trace = []
    status, detail, _ = check(expected, actual, trace)
    for position, line in enumerate(trace):
        print(f"{position:4} {line}")
    print()
    print(f"{status}{': ' + detail if detail else ''}")


def main():
    parser = argparse.ArgumentParser(description="Compare Falcon's packet encoders with the published schemas")
    parser.add_argument("--protocol", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--docs", type=Path, required=True, help="the json folder of the protocol documentation")
    parser.add_argument("--report", type=Path, default=Path("protocol-report.md"))
    parser.add_argument("--strict", action="store_true", help="exit with 1 when a packet mismatches")
    parser.add_argument("--explain", metavar="PACKET", help="print how both encodings of one packet line up")
    arguments = parser.parse_args()

    docs = MojangDocs(arguments.docs)
    index = FalconIndex(arguments.protocol)
    documented = docs.packets()
    implemented = falcon_packets(arguments.protocol, index)

    if arguments.explain:
        expected = merge_length_prefixed(mojang_tokens(docs, documented[arguments.explain][1], "", 0, set()))
        explain(expected, implemented.get(arguments.explain, []))
        return 0
    ids = packet_ids(arguments.protocol / "include" / "Protocol" / "MinecraftPacketIds.h")

    results = []
    optional_notes = []
    byte_notes = []
    for name, tokens in sorted(implemented.items()):
        if name not in documented:
            continue
        expected = merge_length_prefixed(mojang_tokens(docs, documented[name][1], "", 0, set()))
        status, detail, info = check(expected, tokens)
        results.append((name, status, detail, info[VERIFIED], info[HELPERS]))
        optional_notes.extend(f"{name}: {field}" for field in sorted(info[OPTIONAL]))
        byte_notes.extend(f"{name}: {field}" for field in sorted(info[BYTES]))

    id_mismatches = []
    for name, (packet_id, _) in documented.items():
        short = name[:-len("Packet")]
        if short in ids and ids[short] != packet_id:
            id_mismatches.append((name, ids[short], packet_id))

    missing = sorted(name for name in documented if name not in implemented)
    undocumented = sorted(name for name in implemented if name not in documented)
    counts = {status: sum(1 for result in results if result[1] == status)
              for status in ("match", "partial", "mismatch")}

    lines = ["# Protocol check", ""]
    lines.append(f"- Falcon protocol: **{falcon_protocol(arguments.protocol)}**, documentation protocol: "
                 f"**{docs.protocol()}**")
    lines.append(f"- {len(results)} packets compared: **{counts['match']} match**, {counts['partial']} partially "
                 f"checked, **{counts['mismatch']} mismatch**")
    lines.append(f"- {len(id_mismatches)} packet id differences, {len(missing)} documented packets not implemented, "
                 f"{len(undocumented)} implemented packets not documented")
    lines.append("")

    if id_mismatches:
        lines += ["## Packet id differences", "", "| Packet | Falcon | Documentation |", "|---|---|---|"]
        lines += [f"| {name} | {falcon} | {documented_id} |" for name, falcon, documented_id in id_mismatches]
        lines.append("")

    mismatches = [result for result in results if result[1] == "mismatch"]
    if mismatches:
        lines += ["## Mismatches", "", "| Packet | First difference |", "|---|---|"]
        lines += [f"| {name} | {detail} |" for name, _, detail, _, _ in mismatches]
        lines.append("")

    partial = [result for result in results if result[1] == "partial"]
    if partial:
        lines += ["## Partially checked", "", "| Packet | Fields verified | Reason |", "|---|---|---|"]
        lines += [f"| {name} | {len(verified)} | {detail} |" for name, _, detail, verified, _ in partial]
        lines.append("")

    if missing:
        lines += ["## Documented packets not implemented", "", ", ".join(missing), ""]
    if undocumented:
        lines += ["## Implemented packets not in the documentation", "", ", ".join(undocumented), ""]

    if optional_notes:
        lines += ["## Optional fields the documentation lists as required", "",
                  "Falcon writes a presence flag before these fields. Check them against a newer documentation.", ""]
        lines += [f"- {note}" for note in optional_notes]
        lines.append("")

    if byte_notes:
        lines += ["## Compressed bytes", "",
                  "The documentation marks these 8-bit values as compressed. A byte and an unsigned varint are the "
                  "same bytes below 128, so both are accepted.", ""]
        lines += [f"- {note}" for note in byte_notes]
        lines.append("")

    matched = [result for result in results if result[1] == "match"]
    if matched:
        lines += ["## Matching packets", "", ", ".join(name for name, *_ in matched), ""]

    report = "\n".join(lines)
    arguments.report.write_text(report, encoding="utf-8")
    print(report)

    if arguments.strict and (mismatches or id_mismatches):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
