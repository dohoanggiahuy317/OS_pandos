#ifndef NUCL_INITIAL
#define NUCL_INITIAL

#include "../h/const.h"
#include "../h/types.h"
#include "../h/pcb.h"
#include "../h/asl.h"

extern unsigned int processCount;
extern unsigned int softBlockedCount;
extern pcb_PTR readyQueue;
extern pcb_PTR currentProcess;

extern semaphore semaphoreInternal;
extern semaphore semaphoreDevices[MAX_DEVICE_COUNT];

extern cpu_t start_TOD;
extern cpu_t curr_TOD;
extern state_PTR savedExceptionState;

void updateProcessTimeHelper(pcb_PTR process, cpu_t start, cpu_t end);

#endif