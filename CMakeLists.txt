cmake_minimum_required(VERSION 3.25)
project(bench-time CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Put all build products into predictable folders inside the build dir
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(bench-time src/main.cc)
target_include_directories(bench-time PRIVATE src)

if (UNIX AND NOT APPLE)
    # Linux: some toolchains need explicit librt for clock_gettime
    target_link_libraries(bench-time PRIVATE rt)
endif()
