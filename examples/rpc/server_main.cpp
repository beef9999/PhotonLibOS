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

#include <gflags/gflags.h>
#include <photon/common/utility.h>
#include <photon/common/alog.h>
#include <photon/photon.h>
#include <photon/thread/thread11.h>
#include <photon/rpc/rpc.h>

#include "protocol.h"


DEFINE_int32(port, 18888, "Server listen port, 0(default) for random port");
DEFINE_uint32(buf_size, 32768, "buf");

struct ExampleServer {
    std::unique_ptr<photon::rpc::Skeleton> skeleton;
    std::unique_ptr<photon::net::ISocketServer> server;
    uint64_t qps = 0;

    ExampleServer()
            : skeleton(photon::rpc::new_skeleton(655360U)),
              server(photon::net::new_tcp_socket_server()) {
        skeleton->register_service<ReadBuffer>(this);
    }


    int do_rpc_service(ReadBuffer::Request* req, ReadBuffer::Response* resp,
                       IOVector* iov, IStream*) {
        assert(req->buf.size() == (size_t) FLAGS_buf_size);
        resp->buf.assign(req->buf.addr(), req->buf.length());
        qps++;
        return 0;
    }

    int serve(photon::net::ISocketStream* stream) {
        return skeleton->serve(stream);
    }

    int run(int port) {
        if (server->bind(port) < 0)
            LOG_ERRNO_RETURN(0, -1, "Failed to bind port `", port)
        if (server->listen() < 0) LOG_ERRNO_RETURN(0, -1, "Failed to listen");
        server->set_handler({this, &ExampleServer::serve});
        LOG_INFO("Started rpc server at `", server->getsockname());

        photon::thread_create11(&ExampleServer::show_qps, this);

        return server->start_loop(true);
    }

    void show_qps() {
        while (true) {
            photon::thread_sleep(1);
            LOG_INFO(qps);
            qps = 0;
        }
    }
};


int main(int argc, char** argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    int cmp;
    kernel_version_compare("5.15", cmp);
    int ev_engine = cmp >= 0 ? photon::INIT_EVENT_IOURING : photon::INIT_EVENT_EPOLL;
    int ret = photon::init(ev_engine, photon::INIT_IO_NONE);
    DEFER(photon::fini());

    auto s = new ExampleServer();
    s->run(FLAGS_port);
    return 0;
}