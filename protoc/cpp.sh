#!/bin/bash
protoc --cpp_out=. proto_message.proto

protoc --cpp_out=. error_message.proto
protoc --cpp_out=. login_message.proto
protoc --cpp_out=. room_message.proto
protoc --cpp_out=. game_message.proto
protoc --cpp_out=. sync_message.proto