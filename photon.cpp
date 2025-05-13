/*
Copyright 2022 The Photon Authors

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <photon/photon.h>
#include <inttypes.h>

#include "io/fd-events.h"
#include "io/signal.h"
#include "io/aio-wrapper.h"
#include "thread/thread.h"
#include "thread/thread-pool.h"
#include "thread/workerpool.h"
#include "thread/stack-allocator.h"

#ifdef ENABLE_FSTACK_DPDK
#include "io/fstack-dpdk.h"
#endif

#include "io/reset_handle.h"

#ifdef ENABLE_CURL
#include "net/curl.h"
#endif

#include "net/socket.h"
#include "fs/exportfs.h"
#include "common/alog.h"
#include "common/callback.h"
#include <vector>
#include <photon/rpc/rpc.h>
#include "photon_c.h"

namespace photon {

using namespace fs;
using namespace net;

static bool reset_handle_registed = false;
static thread_local uint64_t g_event_engine = 0, g_io_engine = 0;

#define INIT_IO(name, prefix, ...) if (INIT_IO_##name & io_engine)   { if (prefix##_init(__VA_ARGS__) < 0) return -1; }
#define FINI_IO(name, prefix)      if (INIT_IO_##name & g_io_engine) {     prefix##_fini(); }

class Shift {
public:
    uint8_t _n;
    constexpr Shift(uint64_t x) : _n(__builtin_ctz(x)) { }
    operator uint64_t() { return 1UL << _n; }
};

// Try to init master engine with the recommended order
static const Shift recommended_order[] = {
#if defined(__linux__)
    INIT_EVENT_EPOLL, INIT_EVENT_IOURING, INIT_EVENT_EPOLL_NG, INIT_EVENT_SELECT};
#else   // macOS, FreeBSD ...
    INIT_EVENT_KQUEUE, INIT_EVENT_SELECT};
#endif

inline iouring_args mkargs(uint64_t flags, const PhotonOptions& opt) {
    return {
    .is_master          = true,
    .setup_sqpoll       = bool(flags & INIT_EVENT_IOURING_SQPOLL),
    .setup_sq_aff       = bool(flags & INIT_EVENT_IOURING_SQ_AFF),
    .setup_iopoll       = bool(flags & INIT_EVENT_IOURING_IOPOLL),
    .sq_thread_cpu      = opt.iouring_sq_thread_cpu,
    .sq_thread_idle_ms  = opt.iouring_sq_thread_idle_ms,
};   }

static int init_event_engine(uint64_t engine, uint64_t flags, const PhotonOptions& opt) {
#ifdef PHOTON_URING
    auto mee = (engine != INIT_EVENT_IOURING) ?
        new_master_event_engine(engine) :
        new_iouring_master_engine(mkargs(flags, opt));
#else
    auto mee = new_master_event_engine(engine);
#endif
    return fd_events_init(mee);
}

int __photon_init(uint64_t event_engine, uint64_t io_engine, const PhotonOptions& options) {
    if (options.use_pooled_stack_allocator) {
        use_pooled_stack_allocator();
    }
    if (options.bypass_threadpool) {
        set_bypass_threadpool(true);
    }

    if (vcpu_init() < 0)
        return -1;

    const uint64_t ALL_ENGINES =
            INIT_EVENT_EPOLL   | INIT_EVENT_EPOLL_NG |
            INIT_EVENT_IOURING | INIT_EVENT_KQUEUE |
            INIT_EVENT_SELECT  | INIT_EVENT_IOCP;
    if (event_engine & ALL_ENGINES) {
        for (auto x : recommended_order) {
            if ((x & event_engine) && init_event_engine(x, event_engine, options) == 0) {
                goto next;
            }
        }
        LOG_ERROR_RETURN(0, -1, "All master engines init failed");
    }
next:
    if ((INIT_EVENT_SIGNAL & event_engine) && sync_signal_init() < 0)
        return -1;

#ifdef ENABLE_FSTACK_DPDK
    INIT_IO(FSTACK_DPDK, fstack_dpdk);
#endif
    INIT_IO(EXPORTFS, exportfs)
#ifdef ENABLE_CURL
    INIT_IO(LIBCURL, libcurl)
#endif
#ifdef __linux__
    INIT_IO(LIBAIO, libaio_wrapper, options.libaio_queue_depth)
    INIT_IO(SOCKET_EDGE_TRIGGER, et_poller)
#endif
    g_event_engine = event_engine;
    g_io_engine = io_engine;
    if (!reset_handle_registed) {
        pthread_atfork(nullptr, nullptr, &reset_all_handle);
        LOG_DEBUG("reset_all_handle registed ", VALUE(getpid()));
        reset_handle_registed = true;
    }
    return 0;
}

int init(uint64_t event_engine, uint64_t io_engine, const PhotonOptions& options) {
    return __photon_init(event_engine, io_engine, options);
}

static std::vector<Delegate<void>>& get_hook_vector() {
    thread_local std::vector<Delegate<void>> hooks;
    return hooks;
}

void fini_hook(Delegate<void> handler) {
    get_hook_vector().emplace_back(handler);
}

int fini() {
    for (auto h : get_hook_vector()) {
        h.fire();
    }
#ifdef __linux__
    FINI_IO(LIBAIO, libaio_wrapper)
    FINI_IO(SOCKET_EDGE_TRIGGER, et_poller)
#endif
#ifdef ENABLE_CURL
    FINI_IO(LIBCURL, libcurl)
#endif
    FINI_IO(EXPORTFS, exportfs)
#ifdef ENABLE_FSTACK_DPDK
    FINI_IO(FSTACK_DPDK, fstack_dpdk)
#endif

    if (INIT_EVENT_SIGNAL & g_event_engine)
        sync_signal_fini();
    fd_events_fini();
    vcpu_fini();
    g_event_engine = g_io_engine = 0;
    return 0;
}

}

struct RPCProto {
    const static uint32_t IID = 0x1;
    const static uint32_t FID = 0x2;

    struct Request : public photon::rpc::Message {
        photon::rpc::buffer buf;

        PROCESS_FIELDS(buf);
    };

    struct Response : public photon::rpc::Message {
        photon::rpc::buffer buf;

        PROCESS_FIELDS(buf);
    };
};

// thread_local photon::rpc::StubPool* g_stub_pool = nullptr;
photon::WorkPool* g_wp = nullptr;


static photon::rpc::StubPool* get_stub_pool() {
    thread_local photon::rpc::StubPool* stub_pool = nullptr;
    if (stub_pool == nullptr) {
        LOG_INFO("create rpc pool at `", pthread_self());
        stub_pool = photon::rpc::new_stub_pool(-1UL, -1UL);
    }
    return stub_pool;
}

#ifdef __cplusplus
extern "C" {
#endif

int photon_init(uint64_t event_engine, uint64_t io_engine, uint64_t vcpu_num) {
    set_log_output_level(ALOG_FATAL);
    photon::PhotonOptions options;
    options.use_pooled_stack_allocator = true;
    options.bypass_threadpool = true;
    int ret = photon::__photon_init(event_engine, io_engine, options);
    if (ret != 0) {
        return ret;
    }
    g_wp = new photon::WorkPool(vcpu_num, int(event_engine), int(io_engine), 0);
    return 0;
}

int photon_fini() {
    // delete g_stub_pool;
    delete g_wp;
    photon::fini();
    return 0;
}

void async_run(FuncType func, uintptr_t args) {
    g_wp->async_call(new auto([func, args]() {
        func(args);
    }));
}

int rpc(const char* addr, char* req_data, size_t req_size, char* resp_data, size_t resp_size) {
    photon::net::EndPoint ep(addr);
    RPCProto::Request req;
    req.buf.assign(req_data, req_size);
    RPCProto::Response resp;
    resp.buf.assign(resp_data, resp_size);

    int ret = -1;
    auto stub = get_stub_pool()->get_stub(ep, false);
    if (!stub) {
        LOG_ERRNO_RETURN(0, -1, "fail to get stub to `", ep);
    }
    DEFER(get_stub_pool()->put_stub(ep, ret < 0));
    ret = stub->call<RPCProto>(req, resp);
    return ret;
}

void async_rpc(const char* addr, char* req, size_t req_size, char* resp, size_t resp_size,
               FuncType func, uintptr_t args) {
    g_wp->async_call(new auto([=]() {
        int ret = rpc(addr, req, req_size, resp, resp_size);
        if (ret < 0) {
            LOG_ERROR("fail to send rpc");
        }
        func(args);
    }));
}

void photon_usleep(uint64_t usec) {
    photon::thread_usleep(usec);
}


#ifdef __cplusplus
}
#endif