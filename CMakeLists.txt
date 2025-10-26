cmake_minimum_required(VERSION 3.22)
project(FastMoney CXX)

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -mtune=native -flto -DNDEBUG")
set(CMAKE_BUILD_TYPE release)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Required packages
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(OpenSSL REQUIRED)

add_library(AstraLib INTERFACE)
target_include_directories(AstraLib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/external/AstraLib/include
)

add_executable(apiTest ${CMAKE_SOURCE_DIR}/tests/apiManagerTest.cpp ${CMAKE_SOURCE_DIR}/include/simdjson/simdjson.cpp)
target_include_directories( apiTest PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include/simdjson
)

target_link_libraries(apiTest PRIVATE
    Boost::system
    Boost::thread
)
target_link_libraries(apiTest PRIVATE
    AstraLib)
target_link_libraries(apiTest PRIVATE OpenSSL::SSL OpenSSL::Crypto)

add_executable(websocketTest ${CMAKE_SOURCE_DIR}/tests/websocketTest.cpp ${CMAKE_SOURCE_DIR}/include/simdjson/simdjson.cpp)
target_include_directories( websocketTest PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include/simdjson
)

target_link_libraries(websocketTest PRIVATE
    Boost::system
    Boost::thread
)
target_link_libraries(websocketTest PRIVATE
    AstraLib)
target_link_libraries(websocketTest PRIVATE OpenSSL::SSL OpenSSL::Crypto)