option(TimerTest "Build timer test")
option(ThreadPoolTest "Build ThreadPool test")
option(AtomicRingBufferTest "Build ThreadPool test")
option(LoggerTest "Build logger test")

if(TimerTest)
    file(GLOB_RECURSE TESTING_RESOURCE_FOLDERS CONFIGURE_DEPENDS
        ${CMAKE_SOURCE_DIR}/src/testing/isolatedModules/*.cpp
    )
    add_executable(timerTest testing/timerTest.cpp)
    target_include_directories(timerTest PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/isolatedModules
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    )
endif()

if(ThreadPoolTest)
    add_executable(ThreadPoolTest testing/UnitTests/ThreadPoolTest.cpp)

    target_include_directories(ThreadPoolTest PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/utils
        ${Boost_INCLUDE_DIRS}
        ${OPENSSL_INCLUDE_DIR}
    )

endif()

if(AtomicRingBufferTest)
    add_executable(atomicRingBufferTest testing/UnitTests/atomicRingBufferTest.cpp)
    target_include_directories(atomicRingBufferTest PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/utils
        ${Boost_INCLUDE_DIRS}
        ${OPENSSL_INCLUDE_DIR}
        )
endif()

if(LoggerTest) 
    add_executable(loggerTest ${CMAKE_SOURCE_DIR}/tests/LoggerPTest.cpp)
    target_link_libraries(loggerTest AstraLib)
endif()
