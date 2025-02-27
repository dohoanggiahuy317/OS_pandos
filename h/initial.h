#ifndef NUCL_INITIAL
#define NUCL_INITIAL

#include "pcb.h"
#include "asl.h"
#include "const.h"
#include "types.h"

extern unsigned int processCount;
extern unsigned int softBlockedCount;
extern pcb_PTR readyQueue;
extern pcb_PTR currentProcess;

extern semaphore semaphoreInternal;
extern semaphore semaphoreDevices[MAX_DEVICE_COUNT];

cpu_t start_TOD;
state_PTR savedExceptionState;

#endif