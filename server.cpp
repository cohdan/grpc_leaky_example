//
// Created by Dan Cohen on 11/11/2024.
//

#include "server.h"
#include "handlers.h"

#include <chrono>
#include <sstream>
#include <memory>
#include <cstdlib>
#include <iostream>

constexpr int MAX_RETRY_COUNT = 10;

using namespace grpc;
using namespace std::chrono_literals;
using namespace google::protobuf;

using std::endl;
using std::cout;

void server::init_rpc_handlers() {
    // Create the command handlers
    auto _cmd_ping = (handler_base *)handlers_pool::get_pool().allocator().allocate_node();
    _cmd_ping = new (_cmd_ping) handler_server_ping(_completion_queue.get(), _service);

    auto _cmd_get_version = (handler_base *)handlers_pool::get_pool().allocator().allocate_node();
    _cmd_get_version = new (_cmd_get_version) handler_version_get(_completion_queue.get(), _service);

    _cmd_ping->init_rpc_handler();
    _cmd_get_version->init_rpc_handler();
}

void server::handle_requests_queue() {
    void *rpc_tag = nullptr;  // uniquely identifies a request.
    auto rpc_status = false;
    uint16_t retry_count = 0;

    while (true) {
        auto ret = _completion_queue->Next(&rpc_tag, &rpc_status);
        if (!ret) {
            cout << "completion_queue next method indicates that the gRPC server is shutting down, ret=" << ret << ", rpc_status=" << rpc_status << ", did_we_initiate=" << is_server_shutting_down() << endl;
            break;
        }
        if (!rpc_status) {
            cout << "completion_queue next method indicates that an RPC request failed, moving to next request, retry_count=" << retry_count << endl;

            if (rpc_tag) {
                auto rpc_handler = static_cast<handler_base *>(rpc_tag);
                cout << "Failed rpc request details-> " << rpc_handler->get_request_debug_message() << endl;
                rpc_handler->complete_request();
            }

            if (is_server_shutting_down()) {
                cout << "completion_queue next method indicates that the gRPC server is shutting down, ret=" << ret << ", rpc_status=" << rpc_status << endl;
                break;
            }

            if (retry_count < MAX_RETRY_COUNT) {
                ++retry_count;
                std::this_thread::sleep_for(5ms);
            }
            else {
                cout << "Retry count exceeded the configured max, can't recover - killing the agent" << endl;
                ::abort();
            }
            continue;
        }
        retry_count = 0;
        if (!rpc_tag) {
            cout << "invalid RPC request moving to next request" << endl;
            continue;
        }

        static_cast<handler_base *>(rpc_tag)->handle_rpc_request();
    }
}

bool server::init_server() {
    if (_did_init) {
        return true;
    }

    _shutting_down.store(false);
    _service = std::make_shared<ExampleService::AsyncService>();

    auto server_address = "0.0.0.0";
    auto server_port = 6212;
    std::stringstream stream;
    stream << server_address << ":" << server_port;
    std::string server_address_str(stream.str());

    auto max_message_size = 10 * 1024 * 1024;
    ServerBuilder builder;

    // Set max message size.
    builder.SetMaxMessageSize(max_message_size);
    builder.SetMaxSendMessageSize(max_message_size);
    builder.SetMaxReceiveMessageSize(max_message_size);

    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(server_address_str, grpc::InsecureServerCredentials());
    builder.RegisterService(_service.get());

    // Get hold of the completion queue used for the asynchronous communication
    // with the gRPC runtime.

    _completion_queue = builder.AddCompletionQueue();

    // Finally assemble the server.
    _server = builder.BuildAndStart();

    _server_thread = std::make_unique<thread>(std::function<void(server *)>(&server::handle_requests_queue), this);

    init_rpc_handlers();

    _did_init = true;
    return true;
}

void server::close_server() {
    if (!_did_init) {
        return;
    }

    cout << "Closing the grpc server..." << endl;

    _shutting_down.store(true);
    _server->Shutdown();
    _completion_queue->Shutdown();

    if (_server_thread && _server_thread->joinable()) {
        _server_thread->join();
    }

    // Make sure the queue is empty before closing it.
    void* ignored_tag;
    bool ignored_ok;
    while (_completion_queue->Next(&ignored_tag, &ignored_ok)) { }

    _server_thread.reset();
    _server_thread = nullptr;

    _completion_queue.reset();
    _completion_queue = nullptr;

    _service.reset();
    _service = nullptr;

    _did_init = false;
}

server::~server() {
    close_server();
}

