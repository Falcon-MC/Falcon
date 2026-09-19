<p align="center">
	<picture>
		<source media="(prefers-color-scheme: dark)" srcset=".github/logo-white.png">
		<img src=".github/logo.png" alt="Falcon" width="256">
	</picture>
	<br>
	<b>Minecraft: Bedrock Edition server software written from scratch in C++</b>
	<br>
	Not affiliated with Mojang AB.
</p>

<p align="center">
	<img src="https://img.shields.io/badge/minecraft-v1.26.50%20(Bedrock)-56383E" alt="Minecraft">
	<img src="https://img.shields.io/badge/protocol-2193-blue" alt="Protocol">
	<img src="https://img.shields.io/badge/language-C%2B%2B17-00599C" alt="C++17">
	<img src="https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey" alt="Platform">
</p>

## What is this?

Falcon is a Minecraft: Bedrock Edition server built from the ground up in C++17. It does not derive
from any existing server: the protocol, world storage, inventory, movement and gameplay systems are
all reimplemented by hand.

- **Native C++17** - no runtime to install, the server ships as a single self-contained executable
- **Own RakNet implementation** - sockets, reliability layer and connection handshake written here
- **NetherNet transport** - WebRTC signaling and transport, running alongside RakNet
- **Vanilla-like world generation** - noise, density functions, aquifers, caves, biomes and features
- **Behavior pack support** - custom items, blocks, actors and recipes loaded from packs, with a
  QuickJS scripting engine running pack scripts
- **Layered architecture** - network, protocol and server concerns stay separate

## Features

**World**

- LevelDB persistence using the Bedrock key layout, including block entities and pending changes
- Overworld generation with 3D biomes, aquifers, ore veins, caves and surface materials
- Chunk generation *and* population run on worker threads, never on the tick thread
- Chunk streaming ordered by field of view then distance, with reference counted unloading
- Sky light, daylight detector, weather with persistence, and a `dofiretick` game rule

## Building

Falcon needs CMake 3.16+, a C++17 compiler, zlib and OpenSSL. On Windows the reference toolchain is
MSYS2 UCRT64.

```
cmake -B build -G Ninja
cmake --build build
```

## Licensing information

Falcon is licensed under the [GNU Lesser General Public License v3.0](LICENSE), which supplements the
[GNU General Public License v3.0](COPYING). You may use, modify and redistribute it, as long as changes to
Falcon itself stay under the same license.

Falcon is not affiliated with Mojang. All brands and trademarks belong to their respective owners.
Falcon is not a Mojang-approved software, nor is it associated with Mojang.
