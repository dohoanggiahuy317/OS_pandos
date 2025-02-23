#ifndef NUCL_INITIAL
#define NUCL_INITIAL

#include "pcb.h"
#include "asl.h"
#include "const.h"
#include "types.h"

extern unsigned int processCount;
extern unsigned int softBlockedCount;
extern pcb_PTR readyQueue;
extern pcb_PTR currrentProcess;

extern semaphore semaphoreInternal;
extern semaphore semaphore_devices[TOTAL_IO_DEVICES];

#endif