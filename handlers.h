//
// Created by Dan Cohen on 11/11/2024.
//

#ifndef GRPC_EXAMPLE_HANDLERS_H
#define GRPC_EXAMPLE_HANDLERS_H

#include <iostream>
#include <grpcpp/grpcpp.h>
#include <google/protobuf/arena.h>

#include "memory_pool.h"

using namespace google::protobuf;
using namespace example::v1;

using completion_queue_ptr = grpc::ServerCompletionQueue*;
using completion_queue_sptr = std::unique_ptr<grpc::ServerCompletionQueue>;
using service_ptr = std::shared_ptr<ExampleService::AsyncService>;

using std::endl;
using std::cout;

enum request_state_e {
    REQUEST_STATE_INVALID = -1,
    REQUEST_STATE_CREATE = 0,
    REQUEST_STATE_PROCESS = 1,
    REQUEST_STATE_COMPLETE = 2,
    REQUEST_STATE_LAST = 3
};

class handler_base {
public:
    handler_base(completion_queue_ptr completion_queue, service_ptr service) : _completion_queue(completion_queue), _service(service), _arena(nullptr) {
        _state = REQUEST_STATE_CREATE;
    }
    virtual void init_rpc_handler() = 0;
    virtual const std::string get_request_debug_message() = 0;

    virtual void handle_rpc_request() {
        if (_state == REQUEST_STATE_PROCESS) {
            cout << "Recieved rpc: "<< get_request_debug_message() << std::endl;

            if (process_request()) {
                reset_and_prepare_handler_for_next_request();
                _state = REQUEST_STATE_COMPLETE;
            }
        }
        else if (_state == REQUEST_STATE_COMPLETE) {
            complete_request();
        }
    }

    virtual ~handler_base() {}
    virtual void complete_request() {
	handlers_pool::get_pool().allocator().deallocate_node(reinterpret_cast<char*>(this));
    }
protected:
    virtual void reset_and_prepare_handler_for_next_request() {
        handler_base *next_handler = new_rpc_handler();
        next_handler->init_rpc_handler();
    }
    virtual bool process_request() = 0;
    virtual handler_base *new_rpc_handler() = 0;

    void setup_arena_options(google::protobuf::ArenaOptions &arena_options, void *(*alloc_func)(size_t), void (*dealloc_func)(void *,size_t), size_t size) {
        arena_options.block_alloc = alloc_func;
        arena_options.block_dealloc = dealloc_func;
        arena_options.start_block_size = 0;
        arena_options.max_block_size = size;
        arena_options.initial_block = nullptr;
        arena_options.initial_block_size = 0;
    }

    completion_queue_ptr                            _completion_queue;
    service_ptr                                     _service;
    grpc::ServerContext                              _ctx;
    request_state_e                                 _state;
    std::unique_ptr<Arena>                          _arena;

private:
    handler_base(const handler_base &other) = delete;
};

class handler_server_ping : public handler_base {
public:
    handler_server_ping(completion_queue_ptr completion_queue, service_ptr service) : handler_base(completion_queue, service), _responder(&_ctx) {
        memory_allocator::reset(_offset, _buffer, 4096);
        auto alloc = [](size_t size){ return memory_allocator::allocate(_buffer, _offset, size, 4096);};
        auto dealloc = [](void *ptr, size_t size){ cout << "Freeing " << size << " bytes" << endl; memory_allocator::dealloc(_buffer, _offset, size);};
        ArenaOptions options;
        setup_arena_options(options, alloc, dealloc, 4096);
        _arena = std::make_unique<Arena>(options);
    }

    void init_rpc_handler() override {
        _server_ping_request = Arena::Create<ServerPingRequest>(_arena.get());
        _service->RequestServerPing(&_ctx, _server_ping_request, &_responder, _completion_queue, _completion_queue, this);
        _state = REQUEST_STATE_PROCESS;
    }

    const std::string get_request_debug_message() override {
        return "[" + ServerPingRequest::descriptor()->name() + "] " + _server_ping_request->ShortDebugString();
    }
protected:
    bool process_request() override {
        if (_state == REQUEST_STATE_PROCESS) {
            auto *response = Arena::Create<ServerPingResponse>(_arena.get());
	    *response->mutable_pong()->mutable_ping() = _server_ping_request->ping();
	    _ping_counter++;
	    response->mutable_pong()->set_pings_so_far(_ping_counter);
            _responder.Finish(*response, grpc::Status::OK, this);
        }
        return true;
    }

    handler_base *new_rpc_handler() override {
        auto new_cmd = (handler_base *)handlers_pool::get_pool().allocator().allocate_node();
        return new (new_cmd) handler_server_ping(_completion_queue, _service);
    }

private:
    static inline char _buffer[4096] = {};
    static inline size_t _offset = 0;

    ServerPingRequest                                       *_server_ping_request;
    grpc::ServerAsyncResponseWriter<ServerPingResponse>     _responder;
    static inline uint64_t                      	    _ping_counter{0};
};

class handler_version_get : public handler_base {
public:
    handler_version_get(completion_queue_ptr completion_queue, service_ptr service) : handler_base(completion_queue, service),
                                                                                          _responder(&_ctx) {
        memory_allocator::reset(_offset, _buffer, 4096);
        auto alloc = [](size_t size){ return memory_allocator::allocate(_buffer, _offset, size, 4096);};
        auto dealloc = [](void *ptr, size_t size){ memory_allocator::dealloc(_buffer, _offset, size);};
        ArenaOptions options;
        setup_arena_options(options, alloc, dealloc, 4096);
        _arena = std::make_unique<Arena>(options);
    }

    void init_rpc_handler() override {
        _version_get_request = Arena::Create<VersionGetRequest>(_arena.get());
        _service->RequestVersionGet(&_ctx, _version_get_request, &_responder, _completion_queue, _completion_queue, this);
        _state = REQUEST_STATE_PROCESS;
    }

    const std::string get_request_debug_message() override {
        return "[" + VersionGetRequest::descriptor()->name() + "] " + _version_get_request->ShortDebugString();
    }
protected:
    bool process_request() override {
        if (_state == REQUEST_STATE_PROCESS) {
            auto *response = Arena::Create<VersionGetResponse>(_arena.get());
            response->set_version("v1.1");
	    response->set_commit_hash("abc123");
            _responder.Finish(*response, grpc::Status::OK, this);
        }
        return true;
    }

    handler_base *new_rpc_handler() override {
        auto new_cmd = (handler_base *)handlers_pool::get_pool().allocator().allocate_node();;
        return new (new_cmd) handler_version_get(_completion_queue, _service);
    }
private:
    static inline char _buffer[4096] = {};
    static inline size_t _offset = 0;

    VersionGetRequest                                       *_version_get_request;
    grpc::ServerAsyncResponseWriter<VersionGetResponse>     _responder;
};

#endif //GRPC_EXAMPLE_HANDLERS_H
