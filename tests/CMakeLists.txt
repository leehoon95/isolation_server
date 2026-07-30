cmake_minimum_required(VERSION 3.15)
project(isolation_test_project CXX)

find_package(Boost REQUIRED)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

include(GoogleTest)

find_package(Boost REQUIRED)

add_executable(run_unit_tests
    test.cpp
    ../src/Acceptor.cpp
)

target_link_libraries(run_unit_tests PRIVATE
    Boost::boost
    gtest
    gmock
    gtest_main
    isolation_pb
)

gtest_discover_tests(run_unit_tests)