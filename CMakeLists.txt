cmake_minimum_required(VERSION 3.18)
project(fresub LANGUAGES CXX CUDA)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 14)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# Set CUDA architecture - use single target to avoid nvlink warnings
# You can override this by setting CMAKE_CUDA_ARCHITECTURES environment variable
if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
    # Use a single common architecture to reduce nvlink warnings
    # 75 covers most modern GPUs (RTX 20xx, GTX 16xx series)  
    set(CMAKE_CUDA_ARCHITECTURES 75)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Find required packages
find_package(Threads REQUIRED)

# Find exopt external library  
find_library(EXOPT_LIB exopt_lib PATHS ${CMAKE_SOURCE_DIR}/../exopt/build)
find_library(EXOPT_AIG_LIB aig PATHS ${CMAKE_SOURCE_DIR}/../exopt/build/lib/aig)
find_library(EXOPT_CADICAL_LIB cadical PATHS ${CMAKE_SOURCE_DIR}/../exopt/build/lib/cadical)  
find_library(EXOPT_KISSAT_LIB kissat PATHS ${CMAKE_SOURCE_DIR}/../exopt/build/lib/kissat)

# Find exopt include directories
find_path(EXOPT_INCLUDE_DIR synth.hpp PATHS ${CMAKE_SOURCE_DIR}/../exopt/include)
find_path(EXOPT_AIG_INCLUDE_DIR aig.hpp PATHS ${CMAKE_SOURCE_DIR}/../exopt/lib/aig)
find_path(EXOPT_KISSAT_INCLUDE_DIR kissat.h PATHS ${CMAKE_SOURCE_DIR}/../exopt/lib/kissat/src)
find_path(EXOPT_CADICAL_INCLUDE_DIR cadical.hpp PATHS ${CMAKE_SOURCE_DIR}/../exopt/lib/cadical/src)

# Check if exopt was found
if(NOT EXOPT_LIB OR NOT EXOPT_INCLUDE_DIR OR NOT EXOPT_AIG_INCLUDE_DIR)
    message(FATAL_ERROR "exopt library not found. Please build exopt first.")
endif()

# Source files - separate library sources from main
set(CPU_LIB_SOURCES
    src/cpu/fresub_aig.cpp
    src/cpu/window.cpp
    src/cpu/feasibility.cpp
    src/cpu/synthesis_bridge.cpp
    src/cpu/aig_insertion.cpp
    src/cpu/aig_converter.cpp
    src/cpu/conflict.cpp
)

set(CUDA_SOURCES
    src/cuda/resub_kernels.cu
)

# Create library for CPU components (without main.cpp)
add_library(fresub_cpu STATIC ${CPU_LIB_SOURCES})

# Add exopt include directories to fresub_cpu
target_include_directories(fresub_cpu PRIVATE 
    ${EXOPT_INCLUDE_DIR}
    ${EXOPT_AIG_INCLUDE_DIR}
    ${EXOPT_KISSAT_INCLUDE_DIR}
    ${EXOPT_CADICAL_INCLUDE_DIR}
)

# Link exopt library to fresub_cpu
target_link_libraries(fresub_cpu PRIVATE 
    Threads::Threads
    ${EXOPT_LIB}
    ${EXOPT_AIG_LIB}
    ${EXOPT_CADICAL_LIB}
    ${EXOPT_KISSAT_LIB}
)

# Create CUDA library
add_library(fresub_cuda STATIC ${CUDA_SOURCES})
set_target_properties(fresub_cuda PROPERTIES 
    CUDA_SEPARABLE_COMPILATION ON
    CUDA_RESOLVE_DEVICE_SYMBOLS OFF
)

# Main executable - clean resubstitution engine
add_executable(fresub src/cpu/main_resubstitution.cpp)

# Add exopt include directories to fresub executable 
target_include_directories(fresub PRIVATE 
    ${EXOPT_INCLUDE_DIR}
    ${EXOPT_AIG_INCLUDE_DIR}
    ${EXOPT_KISSAT_INCLUDE_DIR}
    ${EXOPT_CADICAL_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(fresub
    fresub_cpu
    Threads::Threads
)

# Note: The nvlink warnings about system libraries (librt, libpthread, libdl) 
# are harmless and expected. They occur because nvlink skips CPU-only libraries
# that are incompatible with CUDA device code, which is correct behavior.

# Compiler flags
target_compile_options(fresub_cpu PRIVATE
    -O3
    -march=native
    -Wall
    -Wextra
    $<$<CONFIG:Debug>:-g>
)

target_compile_options(fresub_cuda PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:
        --use_fast_math
        -O3
        $<$<CONFIG:Debug>:-g -G>
    >
)


# Test executables moved to tests/ directory

# Optional: Add tests
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
install(TARGETS fresub DESTINATION bin)
install(DIRECTORY include/ DESTINATION include/fresub)