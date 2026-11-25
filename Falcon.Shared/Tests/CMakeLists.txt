add_executable(FalconSharedNbtTests NbtTests.cpp)

target_link_libraries(FalconSharedNbtTests PRIVATE FalconShared)

add_test(NAME nbt COMMAND FalconSharedNbtTests)
