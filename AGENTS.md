# AGENTS.md

Instructions for AI coding agents working in the Falcon repository.

If you are a human, read [CONTRIBUTING.md](CONTRIBUTING.md) instead. It is the authoritative document, and
where the two disagree, CONTRIBUTING.md wins.

---

## The project

Falcon is a Minecraft: Bedrock Edition server written from scratch in C++. It is not derived from any other
server: the protocol, storage, gameplay and networking are implemented in this organisation's repositories.

- **Language:** C++17. Do not use newer language features.
- **Build system:** CMake with Ninja, dependencies pulled with `FetchContent`.
- **Platforms:** Windows (MSYS2 UCRT64, gcc), Linux (gcc) and macOS (clang), all built by CI.
- **License:** LGPL-3.0.
- **Branch:** `main`. Protocol upgrades are prepared on a branch named after the upcoming game version.

---

## Required reading

| File                               | Read it for                                                        |
|------------------------------------|--------------------------------------------------------------------|
| [CONTRIBUTING.md](CONTRIBUTING.md) | **Mandatory.** Contribution rules, AI disclosure, closing reasons. |
| [SECURITY.md](SECURITY.md)         | What counts as a vulnerability and how to report one privately.    |
| [README.md](README.md)             | Project overview and supported versions.                           |

---

## Commands

| Task                          | Command               |
|-------------------------------|-----------------------|
| Configure and build           | `./build.sh`          |
| Configure and build (Windows) | `build.bat`           |
| Incremental rebuild           | `cmake --build build` |

The first configure downloads dependencies and needs network access. Build output goes to `build.txt`. A
change that does not compile is worse than no change.

---

## Layout

```
Falcon.Server/
  include/                      Headers, mirroring src/
  src/
    Network/                    Session, login, movement, inventory and gameplay handlers
    Server/                     Server lifecycle, configuration and commands
    World/Actor/                Players, mobs, projectiles, AI and movement systems
    World/Level/                Worlds, chunks, storage, generation, blocks and particles
    World/Level/Block/           Block classes, block actors and block systems
    World/Item/                 Items and item behaviour
    World/Inventory/            Inventories, crafting and item stack requests
    World/Containers/           Container manager models
    Resources/                  Resource packs and pack dependencies
    Scripting/                  JavaScript scripting API for behavior packs
    Falcon/                     Falcon-specific embedding and plugin implementation
  structure/                    BDS 1.26.10 layout evidence and relocation inventory
```

The network transport, protocol packets, NBT and Bedrock data files live in separate `Falcon-MC`
repositories. Never edit their copies under `build/_deps`.

---

## Hard rules

1. **Never invent APIs.** Before calling a function, field, packet, block state or registry entry, search the
   codebase and confirm it exists with that exact signature. If you cannot find it, say so.
2. **Never guess vanilla behaviour.** Packet formats, constants, tick rates, chances and state names must
   match the game. If you cannot verify a value, say which one.
3. **One logical change per branch.** Report unrelated problems you notice instead of fixing them.
4. **No repository-wide reformatting** and no refactors outside the task.
5. **No new dependencies** without an issue first.
6. **No dead code** and no debug logging left in the diff.
7. **Do not weaken security checks**: authentication, packet validation, bounds checks, rate limits. If one is
   in your way, stop and explain.
8. **Do not touch** `.github/workflows/` or release automation unless that is the task.
9. **Do not commit, push or open a pull request** unless the human explicitly asks.

---

## Architecture

- **Classes, not string checks.** Blocks, items and actors with behaviour are classes registered with
  `FALCON_REGISTER_BLOCK`, `FALCON_REGISTER_ITEM` or `FALCON_REGISTER_ACTOR`. The type is decided once, in the
  class `matches()`. Systems call virtual methods and never compare identifiers to find out what something is.
- **Generic over specific.** Prefer `BlockActor::getContainer()` to a chest-only accessor. A function whose
  name contains a concrete type usually has a generic form.
- **Damage.** All player damage goes through `ServerNetworkHandler::hurt` with a `DamageSource`. It handles
  gamerules, the invulnerability window, shields, armor, effects and the totem.
- **Threads.** Game state belongs to the main thread. Chunk workers and the network thread only exchange
  data through queues. Never read or write `Level` chunks or players from a worker.
- **Placement.** `Level::setBlock(position, state, true)` updates the block and its neighbours. Player
  placement does not run the placed block's own update: override `onPlaced` when a block must react to being
  placed.
- **Waterlogging** is block layer 1. `Level::peekBlockPtr` returns `nullptr` for chunks that are not loaded:
  treat that as unknown, not as air.

---

## Best practices

- **Match the surrounding file.** Consistency with neighbouring code beats your preferred style.
- **Search before writing.** Look for an existing helper or system first. A duplicated helper is a review
  comment every time.
- **Prefer early returns** over nested conditions.
- **Comments explain why, never what.** See [Code comments](#code-comments).
- **Fail gracefully on client input.** A malformed packet must never crash the server; reject it and move on.
- **Know the hot paths.** Ticking, chunk streaming, fluids and packet handling run every tick. Avoid
  allocations and full scans there, and measure with `/profiler` before claiming a speedup.

---

## Code comments

A comment says why: a game quirk, a protocol detail, a choice that isn't obvious. If deleting it loses
nothing, delete it.

- **Density:** about one per 50 lines; a file with none is fine. No doc block on every function, no section
  banners, nothing that repeats a name. One line on a function only when its contract surprises: units,
  thread, ownership, meaning of an empty result.
- **Tone:** fragments, lowercase, no final period. No filler (`so`, `which`, `that`, `in order to`): join the
  parts with `:` or `;`. One line, two at most; longer only for a real trap: binary format, protocol order, race.
- **Keep:** the reason for a value or an order, what the game or protocol does that the code copies, traps
  for the next change, short game references (`// sub_14ACCD880`, `// ClientInstance::_tickBuildAction`),
  binary and network formats field by field.
- **Drop:** narration, history, commented-out code, names of other servers or clients, authors, dates,
  tickets, apologies and hedging.
- **TODO** only for a real gap, on one line: `// TODO: catmullrom keyframes, linear for now`.
- A wrong comment is worse than none: fix it or delete it. Keep it right above the line it explains.

```cpp
// air first: the server indexes its palette from it
// undefined "$name" after substitution: the game can't parse it, keeps the default (false)
// some proxies send an empty registry after the real one: keep the names
```

---

## Testing

**You cannot verify gameplay.** You have no Bedrock client and no running server. Never write "tested in-game".
Hand the human a concrete test plan instead: what to do on the server, what to look for, and what would mean
failure.

---

## What you must tell your human

1. **Your exact model name and version**, and which parts of the work you did.
2. **A test plan**, since you cannot run one.
3. **Anything you were unsure about**: an API you could not fully verify, a vanilla value you could not
   confirm, a case you did not handle, a build you could not run.

Do not write pull request descriptions, review replies or issues for the human. Give them the facts to write
from.

---

## Security

Found something exploitable, such as a crash reachable from an unauthenticated client or an authentication
bypass? **Do not open a public issue or pull request, and do not push a proof of concept to a public branch.**
Tell the human directly and point them to [SECURITY.md](SECURITY.md).

---

## Working style

- Read before you write.
- Prefer the smallest change that fixes the problem.
- When the task is ambiguous, ask instead of guessing.
- If you cannot do part of the task, say which part and why.
