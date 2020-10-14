#include <stdio.h>     // for fprintf()
#include <stdlib.h>     // for fprintf()
#include <unistd.h>    // for close(), read()
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/epoll.h> // for epoll_create1(), epoll_ctl(), struct epoll_event
#include <sys/ioctl.h>
#include <string.h>    // for strncmp
#include <time.h>
#include <errno.h>

#define BUF_SIZE 1024*1024*32
static char buf[BUF_SIZE];

#define IVSHMEM_IOCTL_COMM   _IOR('K', 0, int)

const char *ivshmem_dev_file = "/dev/ivshmem";
static int dev_fd;

static void notify_fuzzer(int value) {
  if (dev_fd == -1) {
    return;
  }

  int ret = ioctl(dev_fd, IVSHMEM_IOCTL_COMM, value);
  printf("ioctl ret: %d\n", ret);

  return;
}

static void drain_fd(int fd) {
  while (read(fd, buf, BUF_SIZE) != -1)
    ;

  if (errno != EAGAIN) {
    fprintf(stderr, "drainig fd error, maybe it is not open in nonblocking mode\n");
    return;
  }
}

// TODO: add more to this list
static char *crash_key_words[] = {
                                "BUG:",
                                "WARNING:",
                                "INFO:",
                                "Unable to handle kernel paging request",
                                "general protection fault:",
                                "Kernel panic",
                                "PANIC: double fault",
                                "kernel BUG",
                                "BUG kmalloc-",
                                "divide error:",
                                "invalid opcode:"
                                "UBSAN:",
                                "unregister_netdevice: waiting for",
                                "trusty: panic",
                                "Call Trace:",
                                NULL
};

static int containsCrash(char *msg) {
  int i = 0;

  for (i = 0; crash_key_words[i] != NULL; i++) {
    if (strstr(msg, crash_key_words[i])) {
      return 1;
    }
  }

  return 0;
}


static char *endMsgs[] = {
                          "unable to enumerate USB device",
                          "usb_probe_device: usb_probe_device completed",
                          "usbcore: registered new interface",
                          NULL
};

static int endOfTest(char *msg) {
  int i = 0;

  for (i = 0; endMsgs[i] != NULL; i++) {
    if (strstr(msg, endMsgs[i])) {
      return 1;
    }
  }

  return 0;
}

int main() {
  struct epoll_event event;
  int epoll_fd = epoll_create1(0);

  dev_fd = open(ivshmem_dev_file, O_RDWR);

  if (dev_fd == -1) {
    fprintf(stderr, "Failed to open the device file, maybe the dev node is not created\n");
    return -1;
  }

  if(epoll_fd == -1) {
    fprintf(stderr, "Failed to create epoll file descriptor\n");
    return -1;
  }
  const char *kmsg_file = "/dev/kmsg";
  int kmsg_fd = open(kmsg_file, O_RDONLY | O_NONBLOCK);
  if (kmsg_fd == -1) {
    fprintf(stderr, "Failed to open the kmsg file\n");
    return 1;
  }

  drain_fd(kmsg_fd);

  // notify the fuzzer to start
  notify_fuzzer(0x52);

  event.data.fd = kmsg_fd;
  event.events = EPOLLIN;

  if (-1 == epoll_ctl(epoll_fd, EPOLL_CTL_ADD, kmsg_fd, &event)) {
    fprintf(stderr, "Failed to add kmsg_fd to epoll\n");
    goto fail1;
  }

#define MAX_EVENTS 1
  struct epoll_event events[MAX_EVENTS];
  int event_count;

  while (1) {

    event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    int i = 0;

    for (i = 0; i < event_count; i ++) {

      memset(buf, 0, BUF_SIZE);

      read(events[i].data.fd, buf, BUF_SIZE);

      if (containsCrash(buf)) {
        notify_fuzzer(0x51);
      } else if (endOfTest(buf)){
        const char *cmd = getenv("USB_TEST_CMD");
        int notified = 0;
        if (cmd != NULL) {
          struct timespec ts;
          ts.tv_sec = 0;
          ts.tv_nsec = 500000;
          nanosleep(&ts, &ts);
          // execute the command
          // sleep(3);
          
          printf("Running testing command: %s\n", cmd);
          system(cmd);

          memset(buf, 0, BUF_SIZE);
          event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, 3);

          for (int j = 0; j < event_count; j++) {
            read(events[j].data.fd, buf, BUF_SIZE);

            if (containsCrash(buf)) {
              notified = 1;
              notify_fuzzer(0x51);
            }
          }
        }

        if (notified == 0)  {
          notify_fuzzer(0x50);
        }
      }

      fflush(stdout);
    }
  }

  goto done;

 fail1:
  if(close(epoll_fd)) {
    fprintf(stderr, "Failed to close epoll file descriptor\n");
    return 1;
  }

 done:
  return 0;
}
