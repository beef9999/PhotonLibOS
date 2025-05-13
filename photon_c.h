#ifndef PHOTON_PHOTON_C_H
#define PHOTON_PHOTON_C_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stddef.h>

int photon_init(uint64_t event_engine, uint64_t io_engine, uint64_t vcpu_num);

int photon_fini();

typedef void (*FuncType)(uintptr_t args);

void async_run(FuncType func, uintptr_t args);

int rpc(const char* addr, char* req, size_t req_size, char* resp, size_t resp_size);

void async_rpc(const char* addr, char* req, size_t req_size, char* resp, size_t resp_size,
               FuncType func, uintptr_t args);

void photon_usleep(uint64_t usec);

#ifdef __cplusplus
}
#endif

#endif //PHOTON_PHOTON_C_H
