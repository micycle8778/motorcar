cmake_minimum_required(VERSION 3.14...3.31)
project(
    motorcar
    LANGUAGES CXX C
)

# create compile_commands.json
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

# glfw
FetchContent_Declare(
    glfw3
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    )
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable( glfw3 )

# loggin
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog/
    GIT_TAG v1.15.3
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    )
FetchContent_MakeAvailable( spdlog )

# soloud
FetchContent_Declare(
  soloud
  GIT_REPOSITORY https://github.com/micycle8778/soloud
  GIT_TAG        f8972c168af73a424dafb41e6312eb0ee3d5dd64 
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable( soloud )
## SoLoud doesn't have its own `CMakeLists.txt`, so let's declare a library directly from its sources.
file(GLOB soloud_sources "${soloud_SOURCE_DIR}/src/audiosource/*/*.c*" "${soloud_SOURCE_DIR}/src/c_api/*.c*" "${soloud_SOURCE_DIR}/src/core/*.c*" "${soloud_SOURCE_DIR}/src/filter/*.c*" "${soloud_SOURCE_DIR}/src/backend/miniaudio/soloud_miniaudio.cpp")
add_library(soloud ${soloud_sources})
target_compile_definitions(soloud PRIVATE WITH_MINIAUDIO)
target_include_directories(soloud PUBLIC "${soloud_SOURCE_DIR}/include")
if(APPLE)
    find_library(AudioUnit_LIBRARY AudioUnit)
    find_library(CoreAudio_LIBRARY CoreAudio)
    target_link_libraries(soloud INTERFACE ${AudioUnit_LIBRARY} ${CoreAudio_LIBRARY})
    target_compile_definitions(soloud PRIVATE MA_NO_RUNTIME_LINKING)
endif()
if(WIN32)
    target_compile_definitions( soloud PRIVATE MA_NO_JACK )
endif()

# incbin
FetchContent_Declare(
    incbin
    GIT_REPOSITORY https://github.com/graphitemaster/incbin
    GIT_TAG        22061f51fe9f2f35f061f85c2b217b55dd75310d
    GIT_SHALLOW FALSE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable( incbin )
add_library(incbin STATIC "${incbin_SOURCE_DIR}/incbin.c")
target_include_directories(incbin PUBLIC "${incbin_SOURCE_DIR}")

# webgpu
set(WEBGPU_BACKEND "wgpu" CACHE STRING "WebGPU backend (wgpu or dawn)")
FetchContent_Declare(
  webgpu
  GIT_REPOSITORY https://github.com/yig/WebGPU-distribution
  GIT_TAG        cs425-2025
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable( webgpu )

FetchContent_Declare(
  glfw3webgpu
  GIT_REPOSITORY https://github.com/eliemichel/glfw3webgpu
  GIT_TAG        fdcabcc54b56b50c12c10f5317abf8ae7ac32c29
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable( glfw3webgpu )

# image loading
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb/
  GIT_TAG        f58f558c120e9b32c217290b80bad1a0729fbb2c
  GIT_SHALLOW FALSE
  GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable( stb )
## stb doesn't have a `CMakeLists.txt`. Let's declare a header-only library with an include path.
add_library( stb INTERFACE )
target_include_directories( stb INTERFACE ${stb_SOURCE_DIR} )

# math
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG e7714885929f1dcd2bdc2531cc99d3c1dc6daa18
    GIT_SHALLOW FALSE
    GIT_PROGRESS TRUE
    )
FetchContent_MakeAvailable( glm )

# lua
FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/walterschell/Lua
    GIT_TAG 504ef66d500fa1fb4f1684b6617b01342eee704a
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    )
set(LUA_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable( lua )

FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2
    GIT_TAG v3.5.0
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    )
FetchContent_MakeAvailable( sol2 )

# assimp
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG        fb375dd8c0a032106a2122815fb18dffe0283721
)
FetchContent_MakeAvailable(assimp)
set(ASSIMP_BUILD_TESTS        OFF CACHE INTERNAL "")
set(ASSIMP_BUILD_ZLIB         ON CACHE INTERNAL "")
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE INTERNAL "")
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE INTERNAL "")
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE INTERNAL "")
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE INTERNAL "")

## Declare the engine library
add_library( motorcar STATIC
    src/engine.cpp
    src/input.cpp
    src/gfx.cpp
    src/sound.cpp
    src/resources.cpp
    src/scripts.cpp
    src/ecs.cpp
    src/components.cpp
    src/physics3d.cpp
)

set_target_properties( motorcar PROPERTIES CXX_STANDARD 20 )
## Declare our engine's header path.
## This allows targets that depend on the engine to #include them.
target_include_directories( motorcar PUBLIC src )
target_link_libraries( motorcar PUBLIC glfw soloud spdlog::spdlog webgpu glfw3webgpu glm stb sol2 lua_static incbin assimp::assimp )

add_executable( helloworld demo/helloworld.cpp )
set_target_properties( helloworld PROPERTIES CXX_STANDARD 20 )
target_link_libraries( helloworld PRIVATE motorcar )
target_copy_webgpu_binaries( helloworld )

add_executable( ecs_test demo/ecs_test.cpp )
set_target_properties( ecs_test PROPERTIES CXX_STANDARD 20 )
target_link_libraries( ecs_test PRIVATE motorcar )

# asan + wall + werror
if (MSVC)
    # set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /fsanitize=address")
    # set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}   /fsanitize=address")

    target_compile_options(motorcar PRIVATE /W4)
    add_compile_definitions(NOMINMAX)
else()
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}   -fsanitize=address -fno-omit-frame-pointer")

    target_compile_options(motorcar PRIVATE -Wall)
endif()
