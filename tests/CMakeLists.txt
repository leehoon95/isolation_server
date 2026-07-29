cmake_minimum_required(VERSION 3.15)
project(isolation_test_project CXX)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

include(GoogleTest)

find_package(Boost REQUIRED)

add_executable(run_unit_tests
    testServer.cpp
)

target_link_libraries(run_unit_tests PRIVATE
    gtest
    gmock
    gtest_main
)

gtest_discover_tests(run_unit_tests)