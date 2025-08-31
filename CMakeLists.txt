cmake_minimum_required(VERSION 3.14...3.31)
project(
    motorcar
    LANGUAGES CXX C
)

# asan + wall + werror
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer -Wall -Werror")
set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}   -fsanitize=address -fno-omit-frame-pointer -Wall -Werror")

# create compile_commands.json
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

add_executable( helloworld demo/helloworld.cpp )
set_target_properties( helloworld PROPERTIES CXX_STANDARD 20 )
