#ifndef NUCL_INITIAL
#define NUCL_INITIAL

#include "pcb.h"
#include "asl.h"

extern unsigned int process_count;
extern unsigned int softblocked_count;
extern pcb_PTR ready_queue;
extern pcb_t *curr_process;

extern int semaphore_devices[TOTAL_IO_DEVICES];

#endif