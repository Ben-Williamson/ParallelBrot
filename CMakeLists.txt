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

find_package(glfw3 REQUIRED)

# Ensure Tracy's headers are included
add_executable(ParallelBrot main.cpp)
add_executable(GPUBrot opencl-main.cpp)
add_executable(GLBrot opengl-main.cpp)

# Add Tracy's "public" include directory
target_include_directories(ParallelBrot PRIVATE ${tracy_SOURCE_DIR}/public/tracy)
#target_compile_definitions(ParallelBrot PRIVATE TRACY_ENABLE)
target_link_libraries(ParallelBrot PRIVATE TracyClient)

target_include_directories(GPUBrot PRIVATE ${tracy_SOURCE_DIR}/public/tracy)
#target_compile_definitions(GPUBrot PRIVATE TRACY_ENABLE)
target_link_libraries(GPUBrot PRIVATE TracyClient)

target_link_libraries(GLBrot PRIVATE glfw)

# Enable necessary flags for OpenMP and architecture optimizations
set(CMAKE_CXX_FLAGS "-march=native -fopenmp -lOpenCL -lglfw -lGLEW -lGL")
