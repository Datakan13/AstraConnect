cmake_minimum_required(VERSION 3.22)
project(AstraLib CXX)

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -mtune=native -flto -DNDEBUG")
set(CMAKE_BUILD_TYPE release)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_ALL_TESTS "Enables all regular tests")
option(ENABLE_ALL_PTESTS "Enables all performance tests")
option(ENABLE_DEBUGING "Enable use of debugfunc")
add_library(AstraLib INTERFACE)
target_include_directories(AstraLib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_subdirectory(${CMAKE_SOURCE_DIR}/tests)

if(ENABLE_DEBUGING)
add_compile_definitions(DEBUG_ENABLED)
endif()

if(ENABLE_ALL_TESTS)
    set(TimerTest ON CACHE BOOL "" FORCE)
    set(ThreadPoolTest ON CACHE BOOL "" FORCE)
    set(AtomicRingBufferTest ON CACHE BOOL "" FORCE)
    set(LoggerPTest ON CACHE BOOL "" FORCE)
endif()

