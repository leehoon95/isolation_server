cmake_minimum_required(VERSION 3.15)
project(isolation_test_project CXX)

set(CMAKE_CXX_STANDARD 20)
find_package(Boost REQUIRED)
find_package(Protobuf CONFIG REQUIRED)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

include(GoogleTest)

enable_testing()

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/isolation_pb 
    ${CMAKE_CURRENT_BINARY_DIR}/libs/isolation_pb
)

add_executable(run_unit_tests
    test.cpp
    ../src/Acceptor.cpp
    ../src/Server.cpp
    ../src/sha256.cpp
    ../src/util.cpp
)

target_link_libraries(run_unit_tests PRIVATE
    Boost::boost
    gtest
    gmock
    gtest_main
    isolation_pb
)

gtest_discover_tests(run_unit_tests)