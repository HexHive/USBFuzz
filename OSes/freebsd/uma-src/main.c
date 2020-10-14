#include <stdio.h>     // for fprintf()
#include <unistd.h>    // for close(), read()
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>    // for strncmp
#include <errno.h>

#if 0
#include <sys/epoll.h> // for epoll_create1(), epoll_ctl(), struct epoll_event
#else
#include <sys/event.h>
#endif

#define BUF_SIZE 1024


#if 0

#define IVSHMEM_IOCTL_COMM   _IOR('K', 0, int)
const char *ivshmem_dev_file = "/dev/ivshmem";

#else

#define IVSHMEM_IOCTL_COMM  _IOW('E', 1, u_int32_t)
const char *ivshmem_dev_file = "/dev/ivshmem0";

#endif

static int dev_fd;

static void notify_fuzzer(int value) {
  if (dev_fd == -1) {
    return;
  }

  int ret = ioctl(dev_fd, IVSHMEM_IOCTL_COMM, &value);
  printf("ioctl ret: %d\n", ret);

  return;
}

static void drain_fd(int fd) {
  char buf[BUF_SIZE];
  int n_read;

#if 0
  while ((n_read = read(fd, buf, BUF_SIZE)) != -1) {
#else
    while ((n_read = read(fd, buf, BUF_SIZE)) > 0) {
#endif

      printf("read %d bytes\n", n_read);
      write(STDOUT_FILENO, buf, n_read);
  }

  if (errno != EAGAIN) {
    fprintf(stderr, "drainig fd error, maybe it is not open in nonblocking mode\n");
    return;
  }
}

#if 0
static int time_diff(struct timeval *tv1, struct timeval *tv2) {
  int s_diff = tv2->tv_sec - tv1->tv_sec;
  int us_diff = tv2->tv_usec - tv1->tv_usec;

  return s_diff * 1000000 + us_diff;
}
#endif

#if 0
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
                                NULL
};
#else

static char *crash_key_words[] = {
				  "Fatal trap",
				  "panic:",
				  NULL,
};

#endif

static int containsCrash(char *msg) {
  int i = 0;

  for (i = 0; crash_key_words[i] != NULL; i++) {
    if (strstr(msg, crash_key_words[i])) {
      printf("'%s' contains '%s'\n", msg, crash_key_words[i]);
      return 1;
    }
  }

  return 0;
}


#if 0

static char *endMsgs[] = {
                          "unable to enumerate USB device",
                          "usb_probe_device: usb_probe_device completed with error=",
                          NULL,
};

#else

static char *endMsgs[] = {
			  "[uhub_reattach_port] handling new usb device finished",
                          NULL,
};

#endif
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
  printf("Running User Mode Agent\n");

#if 0
  struct epoll_event event;
  int epoll_fd = epoll_create1(0);
#else
  struct kevent event, tevent;
  int kq;
#endif

  dev_fd = open(ivshmem_dev_file, O_RDWR);

  if (dev_fd == -1) {
    fprintf(stderr, "Failed to open the device file, maybe the dev node is not created\n");
    return -1;
  }

#if 0
  if(epoll_fd == -1) {
    fprintf(stderr, "Failed to create epoll file descriptor\n");
    return -1;
  }
#endif
  
#if 0
  const char *kmsg_file = "/dev/kmsg";
#else
  const char *kmsg_file = "/var/log/messages";
  // const char *kmsg_file = "/dev/klog";
#endif

  int kmsg_fd = open(kmsg_file, O_RDONLY | O_NONBLOCK);
  if (kmsg_fd == -1) {
    fprintf(stderr, "Failed to open the kmsg file\n");
    return 1;
  }

  drain_fd(kmsg_fd);

  printf("============= drain fd =====================\n");
  // notify the fuzzer to start
  notify_fuzzer(0x52);
  printf("============= fuzzer notified =====================\n");

  char buf[BUF_SIZE];

#if 0
  event.data.fd = kmsg_fd;
  event.events = EPOLLIN;

  if (-1 == epoll_ctl(epoll_fd, EPOLL_CTL_ADD, kmsg_fd, &event)) {
    fprintf(stderr, "Failed to add kmsg_fd to epoll\n");
    goto fail1;
  }

  #define MAX_EVENTS 5
  struct epoll_event events[MAX_EVENTS];
  int event_count;

#else

  /* Create kqueue. */
  kq = kqueue();

  if (kq == -1) {
    fprintf(stderr, "kqueue() failed");
    return 1;
  }

  EV_SET(&event, kmsg_fd, EVFILT_VNODE, EV_ADD | EV_CLEAR, NOTE_WRITE,
	 0, NULL);
  int ret = kevent(kq, &event, 1, NULL, 0, NULL); 
  if (ret == -1) {
      fprintf(stderr, "registering kevent error\n");
  }
      
#endif


  printf("waiting for kernel log\n");
  while (1) {

#if 0
    event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    int i = 0;

    for (i = 0; i < event_count; i ++) {


      struct timeval t1 , t2, t3;
      gettimeofday(&t1 , NULL);
      memset(buf, 0, BUF_SIZE);
      gettimeofday(&t2 , NULL);
      /* int bytes_read = */ read(events[i].data.fd, buf, BUF_SIZE);

      /* printf("read %d bytes from %d:%s\n", bytes_read, */
      /* 	     events[i].data.fd, */
      /* 	     buf); */

      gettimeofday(&t3 , NULL);

      if (containsCrash(buf)) {
        printf("===== A crash is detected ======\n");
	notify_fuzzer(0x51);
      }

      if (endOfTest(buf)) {
	printf("===== End of test detected ======\n");
	notify_fuzzer(0x50);
      }

      printf("diff1: %d\n", time_diff(&t1, &t2));
      printf("diff2: %d\n", time_diff(&t2, &t3));
      fflush(stdout);
    }

#else

    // printf("received some kernel log\n");
    ret = kevent(kq, NULL, 0, &tevent, 1, NULL);

    if (ret == -1) {
      fprintf(stderr, "kevent error\n");
    } else {

      // there is something written to the file
      memset(buf, 0, BUF_SIZE);
      /*int n_read =*/ read(kmsg_fd, buf, BUF_SIZE);
      // write(STDOUT_FILENO, buf, n_read);

      if (containsCrash(buf)) {
        printf("===== A crash is detected ======\n");
        notify_fuzzer(0x51);
      }

      if (endOfTest(buf)) {
        printf("===== End of test detected ======\n");
        notify_fuzzer(0x50);
      }

    }
    
#endif
  }

  goto done;


#if 0
 fail1:
  if(close(epoll_fd)) {
    fprintf(stderr, "Failed to close epoll file descriptor\n");
    return 1;
  }
#endif
  
 done:
  return 0;
}
