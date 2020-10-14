#ifndef AFL_H
#define AFL_H

extern unsigned char *afl_area_ptr;


#include <sys/shm.h>
#include "../../config.h"

void afl_init(void);

extern char *usbDescFile;
extern char *usbDataFile;

int run_in_afl(void);

#endif /* AFL_H */
