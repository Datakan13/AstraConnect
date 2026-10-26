cmake_minimum_required(VERSION 3.22)
project(AstraConnect CXX)

# Release unless a build type is given on the command line; the sanitizer builds
# use RelWithDebInfo
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -mtune=native -flto=auto")


set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -DASTRA_SANITIZE=address,undefined or -DASTRA_SANITIZE=thread
set(ASTRA_SANITIZE "" CACHE STRING "Sanitizers to build with, passed to -fsanitize=")
if(ASTRA_SANITIZE)
    # -O1 overrides the build type's level: GCC 11 takes minutes per test file with
    # sanitizers at -O2, and -O1 keeps the stack traces readable
    add_compile_options(-fsanitize=${ASTRA_SANITIZE} -fno-omit-frame-pointer -g -O1)
    add_link_options(-fsanitize=${ASTRA_SANITIZE})
    if(ASTRA_SANITIZE MATCHES "undefined")
        # Make UB fail the test instead of only printing
        add_compile_options(-fno-sanitize-recover=undefined)
    endif()
endif()

# Registers rawWebsocketTest with CTest. Off by default: it needs network access to Binance.
option(ASTRA_LIVE_TESTS "Register the live network tests with CTest" OFF)

# Required packages
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(OpenSSL REQUIRED)

add_library(libraries INTERFACE)
target_include_directories(libraries INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Compiled once and shared by every target that parses JSON
add_library(simdjson OBJECT ${CMAKE_SOURCE_DIR}/include/simdjson/simdjson.cpp)
target_include_directories(simdjson PUBLIC ${CMAKE_SOURCE_DIR}/include)

enable_testing()

# astra_test(<name> LABEL <unit|stress> [TIMEOUT <seconds>] [LIBS <libs>...])
# Builds tests/<name>.cpp and registers it with CTest.
function(astra_test name)
    cmake_parse_arguments(TEST "" "LABEL;TIMEOUT" "LIBS" ${ARGN})
    if(NOT TEST_TIMEOUT)
        set(TEST_TIMEOUT 120)
    endif()
    add_executable(${name} ${CMAKE_SOURCE_DIR}/tests/${name}.cpp)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(${name} PRIVATE libraries ${TEST_LIBS})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS ${TEST_LABEL} TIMEOUT ${TEST_TIMEOUT})
    add_dependencies(offline_tests ${name})
endfunction()

# Builds only what CTest runs: cmake --build build --target offline_tests
add_custom_target(offline_tests)

# Offline
astra_test(topBookTest          LABEL unit)
astra_test(fixedPointTest       LABEL unit   LIBS simdjson)
astra_test(payloadParseTest     LABEL unit   LIBS simdjson)
astra_test(requestSigningTest   LABEL unit   LIBS simdjson Boost::system Boost::thread OpenSSL::SSL OpenSSL::Crypto)
astra_test(backoffTest          LABEL unit   LIBS simdjson Boost::system Boost::thread OpenSSL::SSL OpenSSL::Crypto)
astra_test(ringBufferTryTest    LABEL stress LIBS pthread)
astra_test(depthDecoderTest     LABEL stress LIBS simdjson pthread)
astra_test(sortedBookStressTest LABEL stress LIBS pthread TIMEOUT 300)
astra_test(concurrencyStressTest LABEL stress LIBS simdjson Boost::system Boost::thread OpenSSL::SSL OpenSSL::Crypto TIMEOUT 300)

# Live: places a real order, so it is never registered with CTest
add_executable(apiTest ${CMAKE_SOURCE_DIR}/tests/apiManagerTest.cpp)
target_include_directories( apiTest PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(apiTest PRIVATE
    Boost::system
    Boost::thread
)
target_link_libraries(apiTest PRIVATE
    libraries simdjson)
target_link_libraries(apiTest PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# Live: needs network access to Binance, no API key
add_executable(rawWebsocketTest ${CMAKE_SOURCE_DIR}/tests/rawWebsocketTest.cpp)
target_include_directories(rawWebsocketTest PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(rawWebsocketTest PRIVATE
    libraries
    simdjson
    Boost::system
    Boost::thread
    OpenSSL::SSL OpenSSL::Crypto
)
if(ASTRA_LIVE_TESTS)
    add_test(NAME rawWebsocketTest COMMAND rawWebsocketTest)
    set_tests_properties(rawWebsocketTest PROPERTIES LABELS live TIMEOUT 300)
endif()
