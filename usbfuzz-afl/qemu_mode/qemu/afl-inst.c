#include "afl.h"
#include <sys/types.h>
#include <unistd.h>
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/main-loop.h"
#include "qemu/option.h"
#include "qemu/config-file.h"
#include "hw/qdev.h"
#include "monitor/qdev.h"
#include "qom/object_interfaces.h"
#include "glib.h"


/* from command line options */
char *usbDescFile = NULL;
char *usbDataFile = NULL;
unsigned char *afl_area_ptr = NULL;

static int in_afl = 0;
static pid_t __pid;

// Control PIPE, the fuzzer exerts control
// on the target
// In the target side, we read from it
#define CTRL_FD (FORKSRV_FD)

// status PIPE, the target feeds back to
// the fuzzer
// In the target side, we write to it
#define STUS_FD (FORKSRV_FD + 1)

#define FUZZ_DEV_ID "xxx__usbfuzz1__"

static QEMUTimer timer;
static void afl_timer_cb(void *opaque);

static int try_setup_afl_shm(void);
// static void setup_comm_dev(void);


void start_test(void *opaque);
void stop_test(int val);
void reply_forkserver_handshake(void);

static volatile int test_dev_attached = 0;
static DeviceState* attach_device(const char *param_str, Error **errp);
static int test_device_attached(void);
/* static void detach_device(DeviceState *ds, Error **errp); */
static void detach_device(const char *id, Error **errp);

int run_in_afl(void) {
    if (afl_area_ptr == NULL) {
        in_afl = try_setup_afl_shm();
    } else {
        if (afl_area_ptr == (void *) -1) {
            in_afl = 0;
        }
    }

    return in_afl;
}

static int test_wait = 1000;

void start_test(void *opaque) {
    // DeviceState *ds = NULL;
    Error *err;

    char buff[4];

    // afl_area_ptr[0] = 1;
    printf("Starting to run a test\n");
    if (read(CTRL_FD, buff, 4) != 4) {
        printf("Reading from  Ctrl FD failed, not running with AFL?\n");
        return ;
    }

    pid_t pid =  getpid();
    if (write(STUS_FD, &pid, sizeof(pid)) != sizeof(pid)) {
        printf("Writing to STATUS FD failed, not running with AFL?\n");
        return ;
    }

    if (test_device_attached()) {
        detach_device(FUZZ_DEV_ID, &err);
    }

    memset(afl_area_ptr, 0, MAP_SIZE);
    attach_device("driver=usb-fuzz,id=" FUZZ_DEV_ID, &err);
    AioContext *ctx = qemu_get_aio_context();
    aio_timer_init(ctx, &timer, QEMU_CLOCK_HOST, SCALE_MS, afl_timer_cb, NULL);

    timer_mod(&timer, qemu_clock_get_ms(QEMU_CLOCK_HOST) + test_wait);
}

void stop_test(int val) {

    // Error *err = NULL;
    int status = 0;

    printf("stopping the test\n");

    // detach_device(FUZZ_DEV_ID, &err);

    if (val == 0x51) {
        status = 0x51;
    }

    // g_usleep(1000000);
    if (write(STUS_FD, &status, sizeof(status)) != sizeof(status)) {
        printf("writing to status FD failed, exiting\n");
        exit(-1);
    }
}

void reply_forkserver_handshake(void) {
    if (write(STUS_FD, &__pid, sizeof(__pid)) != sizeof(__pid)) {
        printf("Replying afl with handshake failed, not running with AFL?\n");
    }
}

static int try_setup_afl_shm(void) {
    char *id_str = getenv(SHM_ENV_VAR);

    printf(SHM_ENV_VAR "\n");

    int shm_id;
    if (id_str) {
        printf("[%s]id_str = %s\n", __func__, id_str);
        shm_id = atoi(id_str);
        afl_area_ptr = shmat(shm_id, NULL, 0);

        printf("[%s]afl_area_ptr=%p\n", __func__, afl_area_ptr);

        if (afl_area_ptr == (void*)-1) {
            return 0;
        }
        return 1;
    }

    return 0;
}

static void target_ready_cb(void *opaque) {
    printf("In target ready callback\n");
    reply_forkserver_handshake();
}

static int target_wait = 30000;

void afl_init(void) {

    printf("Initializing AFL\n");

    // setup the shared memory
    if (!run_in_afl()) {
        return;
    }

    printf("Running in AFL mode\n");

    // setup the communication device
    // setup_comm_dev();

    qemu_set_fd_handler(CTRL_FD, start_test, NULL, NULL);
    __pid =  getpid();

    AioContext *ctx = qemu_get_aio_context();
    aio_timer_init(ctx, &timer, QEMU_CLOCK_HOST, SCALE_MS, target_ready_cb, NULL);

    char *target_wait_str = getenv("AFL_QEMU_TARGET_WAIT");
    
    if (target_wait_str) {
        target_wait = atoi(target_wait_str);
    }
    printf("target_wait=%d\n", target_wait);

    timer_mod(&timer, qemu_clock_get_ms(QEMU_CLOCK_HOST) + target_wait);

    char *test_wait_str = getenv("AFL_QEMU_TEST_WAIT");

    if (test_wait_str) {
        test_wait = atoi(test_wait_str);
    }

    printf("test_wait=%d\n", test_wait);
    printf("AFL INTI Completed\n");
    
    /*
    if (write(STUS_FD, &pid, sizeof(pid)) != sizeof(pid)) {
        printf("Writing to STATUS FD failed, not running with AFL?\n");
        return ;
    }
    */
}

static DeviceState* attach_device(const char *param_str, Error **errp) {
    static QemuOpts *opts = NULL;

    if (opts == NULL)
        opts = qemu_opts_parse_noisily(qemu_find_opts("device"), param_str, true);

    test_dev_attached = 1;
    return qdev_device_add(opts, errp);
}

extern void qmp_device_del(const char *id, Error **errp);

static void detach_device(const char *id, Error **errp) {
    test_dev_attached = 0;
    qmp_device_del(id, errp);
}


static int test_device_attached(void) {
    return test_dev_attached;
}

static void afl_timer_cb(void *opaque) {
    stop_test(0);
}


/*
static void setup_comm_dev(void) {
    QemuOpts *opts_shm_obj = NULL;
    QemuOpts *opts_ivshmem = NULL;
    Error *errp = NULL;

    opts_shm_obj = qemu_opts_parse_noisily(qemu_find_opts("object"),
                                           "qom-type=memory-backend-shm,id=shm",
                                           true);
    if (!opts_shm_obj) {
        printf("Failed to parse arguments for host memory object\n");
        exit(-1);
    }

    user_creatable_add_opts(opts_shm_obj, &errp);

    if (errp) {
        printf("Failed to create host memory object\n");
        exit(-1);
    }

    qemu_opts_del(opts_shm_obj);

    opts_ivshmem = qemu_opts_parse_noisily(qemu_find_opts("device"),
                                           "driver=ivshmem-plain,id=ivshmem,memdev=shm",
                                           true);

    if (!opts_ivshmem) {
        printf("Failed to parse options for ivshmem device\n");
        exit(-1);
    }

    qdev_device_add(opts_ivshmem, &errp);

    if (errp) {
        printf("creating ivshmem device failed\n");
        exit(-1);
    }
}
*/
