cmake_minimum_required(VERSION 3.15)
project(isolation_server_project CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()
set(CMAKE_CXX_STANDARD 20)
find_package(Boost REQUIRED)
find_package(Protobuf CONFIG REQUIRED)

add_subdirectory(./libs/isolation_pb)

set(SOURCES
    main.cpp
    redisService.cpp
    Server.cpp
    ClientSocket.cpp
    Acceptor.cpp
    util.cpp
    sha256.cpp
)

add_executable(isolation_server ${SOURCES})

string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S" UTC)
target_compile_definitions(isolation_server PRIVATE BUILD_TIMESTAMP="${BUILD_TIMESTAMP}")

target_link_libraries(isolation_server PRIVATE
    Boost::boost
    isolation_pb
    /usr/local/lib/libredis++.a
    hiredis
)