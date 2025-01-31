cmake_minimum_required(VERSION 3.28)
project(ParallelBrot)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS -march=native)

add_executable(ParallelBrot main.cpp)
