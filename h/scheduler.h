#ifndef SCHEDULER_H
#define SCHEDULER_H


extern void switchContext(pcb_PTR nextProcess);
extern void scheduler();
extern void moveStateHelper(state_PTR source_state, state_PTR destination_state);

#endif
