add_executable(FalconNBTTests NbtTests.cpp)

target_link_libraries(FalconNBTTests PRIVATE FalconNBT)

add_test(NAME FalconNBTTests COMMAND FalconNBTTests)
