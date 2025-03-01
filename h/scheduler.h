#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../h/const.h"
#include "../h/types.h"

extern void switchContext(pcb_PTR nextProcess);
extern void scheduler();
extern void moveStateHelper(state_PTR source_state, state_PTR destination_state);

#endif
