#ifndef NUCL_INITIAL
#define NUCL_INITIAL

#include "../h/const.h"
#include "../h/types.h"

extern int processCount;
extern int softBlockedCount;
extern pcb_PTR readyQueue;
extern pcb_PTR currentProcess;

extern semaphore semaphoreDevices[MAX_DEVICE_COUNT];

extern cpu_t start_TOD;
extern state_PTR savedExceptionState;

extern void updateProcessTimeHelper(pcb_PTR process, cpu_t start, cpu_t end);

#endif