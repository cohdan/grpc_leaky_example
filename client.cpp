//
// Created by Dan Cohen on 11/11/2024.
//
#include <string>
#include <iostream>
#include <grpcpp/grpcpp.h>
#include "protos/example/v1/example.grpc.pb.h"

using std::cout;
using std::endl;

using namespace example::v1;

void usage() {
	cout << "Usage: grpc_client [p|v]" << endl;
        cout << "p - send a ping request to the server" << endl;
        cout << "v - ask the server version" << endl;
}


void version() {
	    cout << "Asking for version..." << endl;
            auto address = std::string("127.0.0.1");
	    auto port = std::string("6212");

            // Starting a grpc client to run the version command.
            std::unique_ptr<ExampleService::Stub>  stub;
            std::shared_ptr<grpc::Channel>      channel;
            std::string server_address = address + ":" + port;
            channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
            stub = ExampleService::NewStub(channel);

            // Sending the ping request.
            VersionGetRequest request;
            VersionGetResponse response;
            grpc::ClientContext context;
            auto ret = stub->VersionGet(&context, request, &response);
            if (ret.ok()) {
                cout << response.DebugString() << endl;
            }
            else {
                std::cout << "VersionGet failed with the following error: error_code=" << ret.error_code() << std::endl;
                std::cout << "Error message: '" << ret.error_message() << "'" << std::endl;
                std::cout << "Error details: '" << ret.error_details() << "'" << std::endl;
            }
}

void ping() {
	cout << "Sending ping..." << endl;
	auto address = std::string("127.0.0.1");
        auto port = std::string("6212");
        // Starting a grpc client to run the ping command.
        std::unique_ptr<ExampleService::Stub>  stub;
        std::shared_ptr<grpc::Channel>      channel;
        std::string server_address = address + ":" + port;
        channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
        stub = ExampleService::NewStub(channel);

	// Sending the ping request.
        ServerPingRequest request;
        request.mutable_ping()->set_ping_generation(time(nullptr));
        cout << "Sending ping request with generation=" << request.ping().ping_generation() << endl;
        ServerPingResponse response;
        grpc::ClientContext context;
        auto ret = stub->ServerPing(&context, request, &response);
        if (ret.ok()) {
            cout << "Received ping response with generation=" << response.pong().ping().ping_generation() << endl;
	    cout << "Server pong:" << response.DebugString() << endl;
        }
        else {
            cout << "Ping failed with the following error: error_code=" << ret.error_code() << endl;
            cout << "Error message: '" << ret.error_message() << "'" << endl;
            cout << "Error details: '" << ret.error_details() << "'" << endl;
        }
}

int main(int argc, char **argv) {
	if (argc != 2) {
		usage();
		return 1;
	}
	std::string command(argv[1]);
	if (command == "p") {
		ping();
	}
	else if (command == "v") {
		version();
	}
	else {
		usage();
		return 1;
	}
	return 0;
}
