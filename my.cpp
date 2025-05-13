#include "photon_c.h"
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
// #include <stdatomic.h>

#include <photon/thread/thread.h>
#include <photon/thread/thread11.h>
#include <photon/thread/workerpool.h>
#include <atomic>

std::atomic_uint64_t count = {0};

void callback(uintptr_t args) {
    auto sem = (photon::semaphore*) args;
    sem->signal(1);
    count++;
}

static void* thread_func(void* arg) {
    while (1) {
        sleep(1);
        printf("%lu\n", count.load());
        count = 0;
    }
    return NULL;
}

int main() {
    printf("hello\n");
    photon_init(1, 0, 16);

    pthread_t tid;
    pthread_create(&tid, NULL, thread_func, NULL);
    pthread_detach(tid);

    photon::WorkPool wp(16, 1, 0, 0);


    for (int i = 0; i < 96; ++i) {
        photon::thread_create11([&]{
            wp.thread_migrate();

            char req[32];
            char resp[65536];
            while (1) {
                photon::semaphore sem;
                async_rpc("10.12.43.17:9527", req, sizeof(req), resp, sizeof(resp), callback, (uintptr_t)&sem);
                sem.wait(1);
            }
        });
    }

    photon::thread_sleep(-1);
    photon_fini();
}