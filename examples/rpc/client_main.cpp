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
#include <chrono>
#include <gflags/gflags.h>
#include <photon/common/alog-stdstring.h>
#include <photon/photon.h>
#include <photon/thread/thread11.h>
#include <photon/rpc/rpc.h>
#include "protocol.h"


DEFINE_int32(port, 18888, "server port");
DEFINE_string(host, "127.0.0.1", "server ip");
DEFINE_int32(concurrency, 8, "concurrency");
DEFINE_uint32(buf_size, 16384, "buf");

uint64_t qps = 0;
uint64_t duration = 0;

static void run_latency_loop() {
    while (true) {
        photon::thread_sleep(1);
        uint64_t lat = (qps != 0) ? (duration / qps) : 0;
        LOG_INFO("latency: ` us", lat);
        qps = duration = 0;
    }
}

struct ExampleClient {
    std::unique_ptr<photon::rpc::StubPool> pool;

    ExampleClient()
            : pool(photon::rpc::new_stub_pool(-1, -1, -1)) {}

    ssize_t RPCRead(photon::net::EndPoint ep) {
        auto stub = pool->get_stub(ep, false);
        if (!stub) {
            LOG_ERRNO_RETURN(0, -1, "fail to get stub");
        }
        char buf[FLAGS_buf_size];
        while (true) {
            auto start = std::chrono::system_clock::now();
            ReadBuffer::Request req;
            req.buf.assign(buf, FLAGS_buf_size);
            ReadBuffer::Response resp;
            int ret = stub->call<ReadBuffer>(req, resp);
            if (ret < 0) {
                LOG_ERRNO_RETURN(0, -1, "read error ", VALUE(ret));
            }
            auto end = std::chrono::system_clock::now();
            duration += std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            qps++;
        }
        return 0;
    }
};

int main(int argc, char** argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    // int cmp;
    // kernel_version_compare("5.15", cmp);
    // int ev_engine = cmp >= 0 ? photon::INIT_EVENT_IOURING : photon::INIT_EVENT_EPOLL;
    int ret = photon::init(photon::INIT_EVENT_EPOLL, photon::INIT_IO_NONE);
    if (ret)
        LOG_ERRNO_RETURN(0, -1, "error init");
    DEFER(photon::fini());

    ExampleClient client;
    auto ep = photon::net::EndPoint(photon::net::IPAddr(FLAGS_host.c_str()), FLAGS_port);

    photon::thread_create11(run_latency_loop);

    for (int32_t i = 0; i < FLAGS_concurrency; ++i) {
        photon::thread_create11(&ExampleClient::RPCRead, &client, ep);
    }

    photon::thread_sleep(-1);
}