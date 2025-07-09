cmake_minimum_required(VERSION 3.10)
project(redis_test_server)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 20)
find_package(Boost REQUIRED)
find_package(protobuf REQUIRED)

#link_directories(/usr/local/lib)

add_subdirectory(./protoc)

set(SOURCES
    main.cpp
    TestAsyncServer.cpp
    redisService.cpp
    Server.cpp
    ClientSocket.cpp
    Acceptor.cpp
    util.cpp
    room.cpp
    roomManager.cpp
)

add_executable(test_server ${SOURCES})
#target_include_directories(test_server PRIVATE ${Protobuf_INCLUDE_DIRS})
target_link_libraries(test_server Boost::boost myproto redis++ hiredis)
#protobuf::protobuf
#protobuf::libprotobuf
#libprotobuf
#protobuf
#${Protobuf_LIBRARIES}