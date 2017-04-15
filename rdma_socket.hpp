#pragma once

#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>
#include <infiniband/verbs.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <cstring>
#include <cassert>
#include <cstdio>

#include <iostream>
#include <exception>

struct rdma_error : public std::exception {
    const char* _what;

    rdma_error(const char* what) : _what(what) {}

    virtual const char* what() const noexcept override {
        return _what;
    }
};

void print_id(struct rdma_cm_id* id) {
    printf("%p\n", id->verbs);
}

class rdma_socket {
    struct rdma_cm_id* _id;
    struct rdma_event_channel* _events;
    struct ibv_pd* _protect;

    rdma_socket()
        : _id(nullptr), _events(rdma_create_event_channel()), _protect(nullptr) {}

public:
    struct ibv_qp_init_attr _qp_attr;

public:
    rdma_socket(enum rdma_port_space type) {
        _events = rdma_create_event_channel();
        int rcode = rdma_create_id(_events, &_id, nullptr, type);
        assert(!rcode);
    }

    // non-copyable
    rdma_socket(const rdma_socket&) = delete;
    rdma_socket& operator=(const rdma_socket&) = delete;

    // moveable
    rdma_socket(rdma_socket&& s)
        : _id(s._id), _events(s._events), _protect(s._protect)
    {
        s._id = nullptr;
        s._events  = nullptr;
        s._protect = nullptr;
    }

    // dtor
    ~rdma_socket() {
        if(_id)
            rdma_destroy_id(_id);
        if(_events)
            rdma_destroy_event_channel(_events);
        if(_protect)
            ibv_dealloc_pd(_protect);
    }


    void bind() {
        this->bind("0.0.0.0", 0);
    }

    void bind(const char* address, short port) {
        struct sockaddr_in addrin;
        addrin.sin_family = AF_INET;
        addrin.sin_port   = htons(port);
        addrin.sin_addr.s_addr = inet_addr(address);
        bzero(&addrin.sin_zero, 8);
        this->bind((struct sockaddr*) &addrin);
    }

    void bind(struct sockaddr* address) {
        int rcode = rdma_bind_addr(_id, address);
        assert(!rcode);
    }

    void listen(int backlog = 16) {
        int rcode = rdma_listen(_id, backlog);
        assert(!rcode);
    }

    rdma_socket accept() {
        struct rdma_cm_event* ev;
        int rcode = rdma_get_cm_event(_events, &ev);
        if(ev->event == RDMA_CM_EVENT_CONNECT_REQUEST) {
            // GOOD
        }
        else {
            // BAD
        }
        rdma_socket new_sock;
        new_sock._id = ev->id;
        new_sock.create_qp();
        
        struct rdma_conn_param params;
        bzero(&params, sizeof(params));
        rcode = rdma_accept(new_sock._id, &params);
        rcode = rdma_ack_cm_event(ev); // ack conn request

        rcode = rdma_get_cm_event(_events, &ev);
        rcode = rdma_ack_cm_event(ev); // ack

        return new_sock;
    }

    void resolve(const char* address, short port) {
        struct sockaddr_in addrin;
        addrin.sin_family = AF_INET;
        addrin.sin_port   = htons(port);
        addrin.sin_addr.s_addr = inet_addr(address);
        bzero(&addrin.sin_zero, 8);
        this->resolve((struct sockaddr*) &addrin);
    }

    void resolve(struct sockaddr* address) {
        const int timeout = 100;
        int rcode = rdma_resolve_addr(_id, nullptr, address, timeout);
        struct rdma_cm_event* ev;
        rcode = rdma_get_cm_event(_events, &ev);
        if(ev->event == RDMA_CM_EVENT_ADDR_RESOLVED) {
            // GOOD
        }
        else if(ev->event == RDMA_CM_EVENT_ADDR_ERROR) {
            // BAD
        }
        else {
            throw rdma_error("Unexpected");
        }
        rcode = rdma_ack_cm_event(ev);
    }

    void connect() {
        int rcode;
        const int timeout = 100;
        rcode = rdma_resolve_route(_id, timeout);

        struct rdma_cm_event* ev;
        rcode = rdma_get_cm_event(_events, &ev);
        rcode = rdma_ack_cm_event(ev);
        struct rdma_conn_param params;
        bzero(&params, sizeof(params));
        params.private_data = nullptr;
        params.private_data = 0;
        params.responder_resources = 8;
        params.initiator_depth     = 8;
        rcode = rdma_connect(_id, &params);
        rcode = rdma_get_cm_event(_events, &ev);
        rcode = rdma_ack_cm_event(ev);
        assert(!rcode);
    }

    void disconnect() {
        int rcode = rdma_disconnect(_id);
        assert(!rcode);
    }

    void create_qp() {
        bzero(&_qp_attr, sizeof(_qp_attr));
        _qp_attr.recv_cq = nullptr;
        _qp_attr.send_cq = nullptr;
        _qp_attr.srq     = nullptr;
        _qp_attr.qp_context = NULL;
        _qp_attr.qp_type    = IBV_QPT_RC;
        _qp_attr.cap.max_send_wr = 8;
        _qp_attr.cap.max_recv_wr = 8;
        _qp_attr.cap.max_send_sge = 8;
        _qp_attr.cap.max_recv_sge = 8;
        _qp_attr.cap.max_inline_data = 8;
        _protect = ibv_alloc_pd(_id->verbs);
        int rcode = rdma_create_qp(_id, _protect, &_qp_attr);
        assert(!rcode);
    }

    sockaddr get_peer_addr() {
        return *rdma_get_peer_addr(_id);
    }

    sockaddr get_local_addr() {
        return *rdma_get_local_addr(_id);
    }

    void post_recv(void * addr, size_t length, struct ibv_mr* mr = nullptr, void * user_data = nullptr) {
        if(mr == nullptr)
            mr = ibv_reg_mr(_protect, addr, length, IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE);
        if(user_data == nullptr)
            user_data = addr;
        int rcode = rdma_post_recv(_id, user_data, addr, length, mr);
        assert(!rcode);
    }

    void post_send(void * addr, size_t length, struct ibv_mr* mr = nullptr, void * user_data = nullptr) {
        if(mr == nullptr)
            mr = ibv_reg_mr(_protect, addr, length, IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE);
        if(user_data == nullptr)
            user_data = addr;
        int rcode = rdma_post_send(_id, user_data, addr, length, mr, 0);
        assert(!rcode);
    }

    struct ibv_wc recv_comp() {
        struct ibv_wc wc;
        int rcode = rdma_get_recv_comp(_id, &wc);
        return wc;
    }

    struct ibv_wc send_comp() {
        struct ibv_wc wc;
        int rcode = rdma_get_send_comp(_id, &wc);
        return wc;
    }
};
