#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../h/const.h"
#include "../h/types.h"
#include "../h/pcb.h"
#include "../h/asl.h"

extern void switchContext(pcb_PTR nextProcess);
extern void scheduler();
extern void moveState (state_PTR source, state_PTR dest);


#endif
