add_executable(FalconProxy main.cpp)
target_link_libraries(FalconProxy PRIVATE FalconNetwork)

if (MINGW)
    target_link_options(FalconProxy PRIVATE -fuse-ld=lld)
endif ()
