//
// Created by Dan Cohen on 11/11/2024.
//

#ifndef GRPC_EXAMPLE_SERVER_H
#define GRPC_EXAMPLE_SERVER_H

#include <atomic>
#include <chrono>
#include <thread>
#include <memory>

#include "example/v1/example.grpc.pb.h"
#include "example/v1/example.pb.h"
#include "handlers.h"
#include "memory_pool.h"

using namespace grpc;
using namespace example::v1;
using namespace google::protobuf;

using std::thread;
using std::unique_ptr;
using std::chrono::system_clock;

class server {
public:
    static server &get_server() {
        static server _server;
        return _server;
    }

    virtual ~server();
    bool init_server();
    void close_server();

private:
    server() : _did_init(false), _server_thread(nullptr), _service(nullptr) {}
    server(const server &other) = delete;
    bool is_server_shutting_down() {
        return _shutting_down.load();
    }

    void init_rpc_handlers();
    void handle_requests_queue();

    std::unique_ptr<thread>                         _server_thread;
    std::unique_ptr<grpc::ServerCompletionQueue>    _completion_queue;
    std::shared_ptr<ExampleService::AsyncService>   _service;
    bool                                            _did_init;
    std::unique_ptr<Server>                         _server;
    std::atomic_bool                                _shutting_down;
    mem_pool                                        _rpc_pool;
};

#endif //GRPC_EXAMPLE_SERVER_H
