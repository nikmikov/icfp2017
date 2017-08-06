cmake_minimum_required(VERSION 3.0)
project(punter)

file(GLOB_RECURSE sources      src/*.cpp src/*.h)

add_executable(punter ${sources})


target_compile_options(punter PUBLIC -std=c++14 -Wall -Wpedantic -g)
