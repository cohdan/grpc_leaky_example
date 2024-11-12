cmake_minimum_required(VERSION 3.10)
project(grpc_example)

SET(CMAKE_PREFIX_PATH "ext_libs/install")
SET(CMAKE_INSTALL_PREFIX "ext_libs/install")

set(CMAKE_CXX_STANDARD 17)

find_package(ZLIB REQUIRED)

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC REQUIRED)
include(FindProtobuf)

include_directories(ext_libs/install/include
	protos)

add_executable(grpc_example main.cpp
        server.cpp
        server.h
        handlers.h
	protos/example/v1/example.grpc.pb.cc
	protos/example/v1/example.pb.cc)

target_link_libraries(grpc_example protobuf::libprotobuf)
target_link_libraries(grpc_example gRPC::grpc++ gRPC::grpc)

add_executable(grpc_client client.cpp
	protos/example/v1/example.grpc.pb.cc
	protos/example/v1/example.pb.cc)
target_link_libraries(grpc_client protobuf::libprotobuf)
target_link_libraries(grpc_client gRPC::grpc++ gRPC::grpc)
