cmake_minimum_required(VERSION 3.24)
project(PlutoFall LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(pluto_core src/game.cpp)
target_include_directories(pluto_core PUBLIC src)

if(WIN32)
    add_executable(PlutoFall WIN32 src/main_win32.cpp)
    target_compile_definitions(PlutoFall PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(PlutoFall PRIVATE pluto_core d3d11 dxgi d2d1 dwrite xinput xaudio2 shell32)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(pluto_core_tests tests/game_tests.cpp)
    target_link_libraries(pluto_core_tests PRIVATE pluto_core)
    add_test(NAME pluto_core_tests COMMAND pluto_core_tests)
endif()
