cmake_minimum_required(VERSION 3.15)
project(isolation_server_project CXX)

set(CMAKE_CXX_STANDARD 20)
find_package(Boost REQUIRED)
find_package(Protobuf CONFIG REQUIRED)

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

target_include_directories(isolation_server PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/src"
)

set_target_properties(isolation_server PROPERTIES OUTPUT_NAME "$isolation-server-app")

target_link_libraries(isolation_server PRIVATE
    Boost::boost
    isolation_pb
    /usr/local/lib/libredis++.a
    hiredis
)