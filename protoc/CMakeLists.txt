cmake_minimum_required(VERSION 3.10)

set(CMAKE_BUILD_TYPE Debug)
find_package(Protobuf REQUIRED)
# set(CMAKE_CXX_STANDARD 20)
# set(CMAKE_PREFIX_PATH "/usr/local")
# set(Protobuf_USE_STATIC_LIBS ON)

file(GLOB PROTO_FILES ./*.proto)

#protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

add_library(myproto STATIC 
    proto_message.pb.cc
    error_message.pb.cc
    login_message.pb.cc
    room_message.pb.cc
    sync_message.pb.cc
)
target_link_libraries(myproto protobuf::libprotobuf)