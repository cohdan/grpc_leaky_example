
#ifndef GRPC_EXAMPLE_MEMORY_POOL_H
#define GRPC_EXAMPLE_MEMORY_POOL_H

#include <cstdlib>
#include <cstdio>
#include <string>
#include <mutex>
#include <iostream>

using std::cout;
using std::endl;

class memory_allocator {
public:
    static void* allocate(char *buffer, size_t &offset, size_t alloc_size, size_t max_size) {
        if (offset + alloc_size > max_size) {
            return nullptr; // Not enough space
        }
        void *ptr = buffer + offset;
        offset += alloc_size;
        return ptr;
    }

    static void dealloc(void *ptr, size_t &offset, size_t dealloc_size) {
        // Do nothing
    }

    static void reset(size_t &offset, char buffer[], size_t size) {
        offset = 0;
        ::memset(buffer, 0x20, size);
    }
};

class mem_pool{
public:
    template <class N>
    struct basic_list{
        basic_list() : head(nullptr), tail(nullptr), size(0), _m() {}
        N* head;
        N* tail;
        uint32_t size;
        std::mutex _m;

        basic_list(basic_list&& o) : _m(){
            o.head = head;
            o.tail = tail;
            o.size = size;
        }

        struct itererator{
            N* head;
            N* tail;
            basic_list* lst;
        };

        N* erase(N* node){
            std::lock_guard lk(_m);
            if(node->next){
                node->next->prev = node->prev;
            }
            if(node->prev){
                node->prev->next = node->next;
            }
            if (head == node){
                head = node->next;
            }
            if (tail == node){
                tail = node->prev;
            }
            size--;
            node->prev = node->next = nullptr;
            node->lst = nullptr;
            return node;
        }

        void push_back(N* node){
            std::lock_guard lk(_m);
            node->prev = node->next = nullptr;
            size++;
            if (tail == nullptr){
                tail = node;
                head = node;
                node->lst = this;
                return;
            }
            tail->next=node;
            node->prev = tail;
            tail = node;
            node->lst = this;
        }

        void clear(){
            while(head){
                erase(head);
            }
        }
    };


    struct mem_node{
        typedef basic_list<mem_node> list;
        mem_node *next, *prev;
        list* lst;
        uint64_t index;
    };

    mem_pool() : _mutex(), _count(0), _size(0), _allocated(nullptr), _free_list(), _allocated_list(), _did_init(false), _name("unnamed_pool") {}

    void init(uint64_t size, uint64_t count, const std::string& name){
        std::unique_lock<std::mutex> g(_mutex);
        if(_did_init) {
            return;
        }
        _size = size;
        _count = count;
        _allocated = (char*)malloc(count * size);
        _nodes = new mem_node[count];

        for (int i = 0 ; i < count ; i ++){
            _nodes[i].index = i;
            _nodes[i].next = nullptr;
            _nodes[i].prev = nullptr;
            _free_list.push_back(_nodes+i);
        }

        if (!name.empty()) {
            _name = name;
        }
        _did_init = true;
    }

    char* allocate_node(){
        std::unique_lock<std::mutex> g(_mutex);
        if (_free_list.head == nullptr){
            return nullptr;
        }
        mem_node* n = _free_list.erase(_free_list.head);
        _allocated_list.push_back(n);

        return _allocated + (_size * n->index);
    }

    bool deallocate_node(char* ptr){
        std::unique_lock<std::mutex> g(_mutex);
        uint64_t index = (ptr - _allocated) / _size;
        if (_nodes[index].lst == &_allocated_list){
            _allocated_list.erase(_nodes + index);
            _free_list.push_back(_nodes + index);
            return true;
        } else {
            // not allocated
            return false;
        }
    }

    void close(){
        std::unique_lock<std::mutex> g(_mutex);
        if (!_did_init){
            return;
        }

        _free_list.clear();
        _allocated_list.clear();

        free(_allocated);
        delete[] _nodes;
        _allocated = nullptr;
        _nodes = nullptr;
        _did_init = false;
    }

protected:
    bool _did_init;
    std::string _name;
    char* _allocated;
    mem_node* _nodes;
    mem_node::list _free_list;
    mem_node::list _allocated_list;
    uint64_t _size;
    uint64_t _count;
    std::mutex _mutex;
};

class handlers_pool {
public:
    static handlers_pool &get_pool() {
        static handlers_pool the_pool;
        return the_pool;
    }

    mem_pool &allocator() {
        return *_pool;
    }

private:
    handlers_pool() {
        _pool = new mem_pool();
        _pool->init(2048, 100, "handlers");
    }
    ~handlers_pool() {
        _pool->close();
        delete _pool;
        _pool = nullptr;
    }
    mem_pool *_pool;
};

#endif // GRPC_EXAMPLE_MEMORY_POOL_H
