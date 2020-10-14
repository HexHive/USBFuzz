/*
 * QEMU Host Memory Backend by shared memory
 *
 * Hui Peng <peng124@purdue.edu>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */
#include "qemu/osdep.h"
#include "sysemu/hostmem.h"
#include "qapi/error.h"
#include "qom/object_interfaces.h"
#include <sys/ipc.h>
#include <sys/shm.h>

#include "afl.h"

#define TYPE_MEMORY_BACKEND_SHM "memory-backend-shm"


#ifndef MAP_SIZE
#define MAP_SIZE_POW2       16
#define MAP_SIZE            (1 << MAP_SIZE_POW2)
#endif

extern void
memory_region_init_ram_nomigrate_with_ptr(MemoryRegion *mr,
                                          Object *owner,
                                          const char *name,
                                          void *ptr,
                                          uint64_t size,
                                          Error **errp);

static void
ram_backend_shm_alloc(HostMemoryBackend *backend, Error **errp)
{
    char *path;
    int shmid;

    path = object_get_canonical_path_component(OBJECT(backend));


    if (!run_in_afl()) {
        printf("Not running in AFL, create a mock afl_area_ptr");
        shmid = shmget(IPC_PRIVATE, MAP_SIZE, IPC_CREAT | IPC_EXCL | 0600);
        afl_area_ptr = shmat(shmid, NULL, 0);
        printf("mock afl_area_ptr: 0x%p\n", afl_area_ptr);

    } else {
        printf("afl_area_ptr: 0x%p\n", afl_area_ptr);
    }

    memory_region_init_ram_nomigrate_with_ptr(&backend->mr,
                                              OBJECT(backend),
                                              path,
                                              afl_area_ptr,
                                              MAP_SIZE, errp);
    g_free(path);
}

static void
shm_backend_class_init(ObjectClass *oc, void *data)
{
    HostMemoryBackendClass *bc = MEMORY_BACKEND_CLASS(oc);

    bc->alloc = ram_backend_shm_alloc;
}

static const TypeInfo shm_backend_info = {
    .name = TYPE_MEMORY_BACKEND_SHM,
    .parent = TYPE_MEMORY_BACKEND,
    .class_init = shm_backend_class_init,
};

static void register_types(void)
{
    type_register_static(&shm_backend_info);
}

type_init(register_types);
