cmake_minimum_required(VERSION 3.28)
project(ParallelBrot)

set(CMAKE_CXX_STANDARD 17)

Include(FetchContent)

FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(tracy)

# Ensure Tracy's headers are included
add_executable(ParallelBrot main.cpp)

# Add Tracy's "public" include directory
target_include_directories(ParallelBrot PRIVATE ${tracy_SOURCE_DIR}/public/tracy)

# Define TRACY_ENABLE at compile time
target_compile_definitions(ParallelBrot PRIVATE TRACY_ENABLE)

# **Link the Tracy client library**
target_link_libraries(ParallelBrot PRIVATE TracyClient)

# Enable necessary flags for OpenMP and architecture optimizations
set(CMAKE_CXX_FLAGS "-march=native -fopenmp")
