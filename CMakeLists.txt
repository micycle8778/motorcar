cmake_minimum_required(VERSION 3.14...3.31)
project(
    motorcar
    LANGUAGES CXX C
)

add_executable( helloworld demo/helloworld.cpp )
set_target_properties( helloworld PROPERTIES CXX_STANDARD 20 )
