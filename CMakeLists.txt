cmake_minimum_required(VERSION 3.10)
project(redis_test_server)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 20)
find_package(Boost REQUIRED)
find_package(protobuf REQUIRED)


add_subdirectory(./isolation_pb)

set(SOURCES
    main.cpp
    redisService.cpp
    Server.cpp
    ClientSocket.cpp
    Acceptor.cpp
    util.cpp
    sha256.cpp
)

add_executable(test_server ${SOURCES})
target_link_libraries(test_server Boost::boost myproto redis++ hiredis)