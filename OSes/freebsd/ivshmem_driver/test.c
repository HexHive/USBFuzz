#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define IVSHMEM_IOCTL_COMM _IOW('E', 1, u_int32_t)

int main() {
  char *file_path = "/dev/ivshmem0";

  int fd = open(file_path, O_RDONLY);

  if (fd < 0) {
    printf("File Open Error\n");
    exit(-1);
  }

  u_int32_t i = 0x0a;

  while (1) {  
    printf("-=-=-= before i = %d -=-=-=\n", i);
    int ret = ioctl(fd, IVSHMEM_IOCTL_COMM, &i);
    printf("-=-=-= after i = %d -=-=-=\n", i);
    printf("ret=%d\n", ret);
    sleep(2);

    i++;

    if (i > 100) {
      i = 0;
    }
  }
  
  close(fd);
  return 0;
}
