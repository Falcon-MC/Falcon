<p align="center">
	<picture>
		<source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/Falcon-MC/Falcon/main/.github/logo-white.png">
		<img src="https://raw.githubusercontent.com/Falcon-MC/Falcon/main/.github/logo.png" alt="Falcon" width="200">
	</picture>
	<br>
	<b>Falcon NBT</b>
	<br>
	Named Binary Tag library for Minecraft written in C++17
</p>

<p align="center">
	<img src="https://img.shields.io/badge/language-C%2B%2B17-00599C" alt="C++17">
</p>

## What is this?

The NBT implementation used by [Falcon](https://github.com/Falcon-MC/Falcon), as a standalone library with
no dependency on the protocol or the server.

- **`Tag`** - a single NBT value of any of the 13 tag types. Compounds keep their insertion order.
- **`NbtIo`** - reads and writes tags in three encodings selected with `NbtVariant`:
  - `BigEndian` - Java Edition layout
  - `LittleEndian` - Bedrock world storage and `.dat` files
  - `Network` - Bedrock packets: ints and longs are zigzag varints, strings and the root name use an unsigned
    varint length, every array and list length is a zigzag varint
- **`BinaryStream` / `ReadOnlyBinaryStream`** - the byte buffer the codecs read from and write to, with
  `EncodingSettings` limits (maximum list size, byte array size and string length) for untrusted input
- **`json::Value`** - the JSON parser shared by the libraries that depend on this one

Nesting is limited to `NbtIo::MAX_DEPTH` (16) levels below the root on both read and write.

## Usage

The library is a plain CMake target named `FalconNBT`. The easiest way to use it is `FetchContent`:

```cmake
include(FetchContent)

FetchContent_Declare(
    falcon_nbt
    GIT_REPOSITORY https://github.com/Falcon-MC/NBT.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(falcon_nbt)

target_link_libraries(your_target PRIVATE FalconNBT)
```

```cpp
#include "Core/NBT/NbtIo.h"

Tag state = Tag::ofCompound();
state.putString("name", "minecraft:stone");
state.put("states", Tag::ofCompound());

BinaryStream stream;
NbtIo::writeTag(stream, state, NbtVariant::LittleEndian);

ReadOnlyBinaryStream input(stream.getBuffer());
Tag decoded = NbtIo::readTag(input, NbtVariant::LittleEndian);
```

## Building

Only CMake 3.16+ and a C++17 compiler are required. Tests are built when the project is the top level
project (`FALCON_NBT_BUILD_TESTS`).

```
cmake -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

## License

LGPL-3.0, see [LICENSE](LICENSE) and [COPYING](COPYING).
