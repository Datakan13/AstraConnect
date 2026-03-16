cmake_minimum_required(VERSION 3.22)
project(AstraConnect CXX)
set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -mtune=native -flto=auto")


set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Required packages
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(OpenSSL REQUIRED)

add_library(libraries INTERFACE)
target_include_directories(libraries INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(apiTest ${CMAKE_SOURCE_DIR}/tests/apiManagerTest.cpp ${CMAKE_SOURCE_DIR}/include/simdjson/simdjson.cpp)
target_include_directories( apiTest PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(apiTest PRIVATE
    Boost::system
    Boost::thread
)
target_link_libraries(apiTest PRIVATE
    libraries)
target_link_libraries(apiTest PRIVATE OpenSSL::SSL OpenSSL::Crypto)

add_executable(topBookTest ${CMAKE_SOURCE_DIR}/tests/topBookTest.cpp)
target_include_directories(topBookTest PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(topBookTest PRIVATE
    libraries
)
