cmake_minimum_required(VERSION 3.15)
project(isolation_server_project CXX)

set(CMAKE_CXX_STANDARD 20)
find_package(Boost REQUIRED)
find_package(Protobuf CONFIG REQUIRED)

message(STATUS "Build server: ${SERVER_NAME_VAR}")
message(STATUS "Redis server host: ${REDIS_SERVER_HOST_VAR}")
message(STATUS "Redis server port: ${REDIS_SERVER_PORT_VAR}")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Config.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Config.h"
)

option(SET_TEST "build with unit tests" OFF)

if(SET_TEST)
    include(CTest)
    enable_testing()
    add_subdirectory(./tests)
endif(SET_TEST)

add_subdirectory(./libs/isolation_pb)

set(SOURCES
    src/main.cpp
    src/redisService.cpp
    src/Server.cpp
    src/ClientSocket.cpp
    src/Acceptor.cpp
    src/util.cpp
    src/sha256.cpp
)

add_executable(isolation_server ${SOURCES})

set_target_properties(isolation_server PROPERTIES OUTPUT_NAME "${SERVER_NAME_VAR}-app")

target_link_libraries(isolation_server PRIVATE
    Boost::boost
    isolation_pb
    /usr/local/lib/libredis++.a
    hiredis
)