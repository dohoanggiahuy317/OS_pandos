#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "../h/scheduler.h"
#include "../h/interrupts.h"
#include "../h/initial.h"
#include "/usr/include/umps3/umps/libumps.h"


void switchContext(pcd_PTR new_process) {
    currentProcess = new_process; /* setting the Current Process to curr_proc */
    STCK(start_tod); /* updating start_tod with the value on the Time of Day Clock, as this is the time that the process will begin executing at */
    LDST(&(currentProcess->p_s)); /* loading the processor state for the processor state stored in pcb of the Current Process */
}


void scheduler() {
	currentProcess = removeProcQ(&readyQueue); /* removing the pcb from the head of the ReadyQueue and storing its pointer in currentProc */
	if (currentProcess != NULL){ /* if the Ready Queue is not empty */
		setTIMER(PLT); /* loading five milliseconds on the processor's Local Timer (PLT) */
		switchContext(currentProcess); /* invoking the internal function that will perform the LDST on the Current Process' processor state */
	}

	/* NOW THE ReadyQueue is empty. */
	if (processCount == INIT_PROCESS_CNT){ /* if the number of started, but not yet terminated, processes is zero */
		HALT(); /* invoking the HALT() function to halt the system and print a regular shutdown message on terminal 0 */
	}
	
	if ((procCnt > INIT_PROCESS_CNT) && (softBlockCnt > INIT_SOFT_BLOCK_CNT)){ /* if the number of started, but not yet terminated, processes is greater than zero and there's at least one such process is "blocked" */
		setSTATUS(ALLOFF | IMON | IECON); /* enabling interrupts for the Status register so we can execute the WAIT instruction */
		setTIMER(INF_TIME); /* loading the PLT with a very large value so that the first interrupt that occurs after entering a WAIT state is not for the PLT */
		WAIT(); /* invoking the WAIT() function to idle the processor, as it needs to wait for a device interrupt to occur */
	}

	/* If procCnt is zero, there are no processes at all, so the system halts.
If procCnt > 0 and softBlockCnt > INITIALSFTBLKCNT, some process is waiting for an event, so the system idles (by calling WAIT) until an interrupt occurs.
But if procCnt > 0 and softBlockCnt is exactly at its initial count (softBlockCnt == INITIALSFTBLKCNT), that means there are processes that have been started and not terminated, yet none of them are blocked waiting for an event. And since the Ready Queue is empty, it means these processes are not in a runnable state either. This situation is unexpected and implies a deadlock: processes exist, but none can proceed or be unblocked. That's why the OS calls PANIC. */
	PANIC(); /* invoking the PANIC() function to stop the system and print a warning message on terminal 0 */
}
