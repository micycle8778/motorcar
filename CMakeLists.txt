cmake_minimum_required(VERSION 3.14...3.31)
project(
    motorcar
    LANGUAGES CXX C
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -fsanitize=address -fno-omit-frame-pointer")
endif()

add_executable( helloworld demo/helloworld.cpp )
set_target_properties( helloworld PROPERTIES CXX_STANDARD 20 )
