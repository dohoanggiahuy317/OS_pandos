/**********************************************************************************************
 * scheduler.c
 * 
 * @brief
 * This file contains essential routines for process management in the operating system.
 * It provides two main functionalities:
 * Context Switching: The switchContext function enables the processor to switch
 * from executing one process to another by saving the current time and loading the
 * state of the target process. This is critical to implementing preemptive multitasking.
 *
 * Process Scheduling: The scheduler function manages which process should be executed 
 * next. It removes a process from the ready queue, sets it as the current process, and 
 * activates it by loading its state. If the ready queue is empty, the scheduler handles 
 * the situation by:
 * - Halting the system when no processes remain,
 * - Allowing the system to wait if processes are blocked on events (via soft-block count),
 * - Detecting a deadlock if there are processes available but none can execute.
 *
 * Without proper context switching and scheduling, processes could not share the CPU effectively, leading 
 * to poor resource utilization and system instability.
 *
 * @def
 * - switchContext(pcb_PTR target_process)
 * The purpose of switching contexts is to save the current execution state, update the
 * current process pointer, record the current time so that it can resume execution. 
 *
 * @note
 * In the scheduler (LAST CASE), if the ready queue is empty and there are still processes in the system 
 *  but no processes are ready to run, it indicates that all processes 
 * are either waiting for some event or are in a state that prevents further progress.
 * When softBlockedCount is 0 , this signals a deadlock 
 * because processes are neither ready to execute nor waiting for an interrupt or event to free them.
 * In such a condition, the system calls PANIC() to halt operations as the system cannot proceed.
 *
 * @author
 * JaWeee Do
 **********************************************************************************************/
#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "../h/scheduler.h"
#include "../h/interrupts.h"
#include "../h/exceptions.h"
#include "../h/initial.h"
#include "/usr/include/umps3/umps/libumps.h"




/*********************************************************************************************
 * updateProcessTimeHelper
 * 
 * @brief
 * This function move the info from one state to another state
 * 
 * @protocol
 * 1. copy all the field
 * 2. copy all the general purpose registers
 * 
 * @note
 * as in C is shallow copy, we move the state from the source to the destination by carefully
 * copying each field of the state structure.
 * We also have to take care all thesysCallOutRangeHandler(); general purpose registers associated with the state
 * 
 * @param source_state: the source state
 * @param destination_state: the destination state
 * @return void
*********************************************************************************************/
void moveStateHelper(state_PTR source_state, state_PTR destination_state){

    /* Step 1: copy all the field */
	destination_state->s_entryHI = source_state->s_entryHI;
	destination_state->s_cause = source_state->s_cause;
	destination_state->s_status = source_state->s_status;
	destination_state->s_pc = source_state->s_pc;

	/* Step 2: copy the register */
    int i;
	for (i = 0; i < STATEREGNUM; i++){ 
		destination_state->s_reg[i] = source_state->s_reg[i];
	}
}


/**********************************************************************************************
 * switchContext
 * 
 * @brief
 * This function is used to switch the context of the current process.
 * The function takes the target process as an argument and sets the current process to the 
 * target process. It then saves the current time and loads the state of the target process.
 *
 * @def switchContext(pcb_PTR target_process)
 * 
 * @purpose
 * - Change the execution context from one process to another.
 * - Record the current time for accounting or tracking purposes.
 * - Resume execution of the new process by loading its state.
 *
 * @protocol
 * 1. Set the current process to the target process.
 * 2. Save the current time using the system timer (STCK).
 * 3. Load the target process's state using LDST.
 * 
 * @param pcb_PTR target_process: pointer to the process to be switched in.
 * @return void
 * **********************************************************************************************/
void switchContext(pcb_PTR target_process) {
    /* Step 1: set the process */
    currentProcess = target_process;

    /* Step 2: save the current time */
    STCK(start_TOD);

    /* Step 3: load the state of the target process */
    LDST(&(target_process->p_s));
}


/**********************************************************************************************
 * scheduler
 * 
 * @brief
 * This function schedules the next process to run by dispatching the next process from
 * the Ready Queue. The scheduling routine ensures that the processor executes the available
 * tasks in a fair and orderly manner.
 *
 * @protocol
 * 1. If the Ready Queue is not empty:
 *    - Remove the process from the head of the Ready Queue.
 *    - Set currentProcess to the removed process.
 *    - Load 5 milliseconds on the Programmable Interval Timer (PLT).
 *    - Load the state of the current process to resume its execution.
 * 2. If the Ready Queue is empty:
 *    - If processCount is 0:
 *         - There are no processes in the system; the system halts via HALT().
 *    - If softBlockedCount is >0:
 *         - Some processes are waiting for an event; the system enables interrupts,
 *           disables the PLT by loading a very large time value, and waits for an event.
 *    - Otherwise:
 *         - A deadlock has been detected; the system cannot make progress and PANIC() is invoked.
 *
 * Note (Deadlock Explanation):
 * When the Ready Queue is empty and softBlockedCount is 0 while there are still processes
 * in the system (processCount > 0), it indicates that all processes are neither ready nor waiting
 * for an external event. This scenario represents a deadlock, as no processes can make progress.
 * In this case, the system calls PANIC() to indicate an unrecoverable system state.
 * 
 * @return void
 * **********************************************************************************************/

void scheduler() {
   /*  pcb_PTR next_process; */  /* pointer to next process to run */

    /* If the Ready Queue is not empty, dispatch the next process */\
    /* if (emptyProcQ(readyQueue)) { */
        /* Ready Queue is empty */
/*         if (processCount == 0) {
 */            /* No processes in the system; halt. */
/*             HALT();
        }
        else if (softBlockedCount > 0) {
 */            /* There are processes in the system but some are blocked waiting for an event.
               Prepare to wait:
                 - Enable interrupts in the status register.
                 - Disable the PLT by loading it with a very large value (INF_TIME).
                 - Execute the WAIT instruction to wait for an event.
            */
/*             setSTATUS(ALLOFF | IMON | IECON);
            setTIMER(INF_TIME);
            WAIT();
        }
        else {
 */            /* Deadlock detected */
/*             PANIC();
        }
        
    } else {
 */        /* Step 1: remove the process from the head of the Ready Queue */
/*         next_process = removeProcQ(&readyQueue);
        currentProcess = next_process;
        
 */        /* Step 2: load 5 milliseconds on the PLT */
/*         LDIT(PLT_TIME_SLICE);
 */        
        /* Step 3: load the state of the current process */
/*         LDST(&(currentProcess->p_s));
    }
 */


    currentProcess = removeProcQ(&readyQueue); /* removing the pcb from the head of the ReadyQueue and storing its pointer in currentProc */
	if (currentProcess != NULL){ /* if the Ready Queue is not empty */
		setTIMER(5000); /* loading five milliseconds on the processor's Local Timer (PLT) */
		switchContext(currentProcess); /* invoking the internal function that will perform the LDST on the Current Process' processor state */
	}

	/* We know the ReadyQueue is empty. */
	if (processCount == 0){ /* if the number of started, but not yet terminated, processes is zero */
		HALT(); /* invoking the HALT() function to halt the system and print a regular shutdown message on terminal 0 */
	}
	
	if ((processCount > 0) && (softBlockedCount > 0)){ /* if the number of started, but not yet terminated, processes is greater than zero and there's at least one such process is "blocked" */
		setSTATUS(ALLOFF | IMON | IECON); /* enabling interrupts for the Status register so we can execute the WAIT instruction */
		setTIMER(INF_TIME); /* loading the PLT with a very large value so that the first interrupt that occurs after entering a WAIT state is not for the PLT */
		WAIT(); /* invoking the WAIT() function to idle the processor, as it needs to wait for a device interrupt to occur */
	}

	/* A deadlock situation is occurring (i.e., procCnt > 0 && softBlockCnt == 0) */
	PANIC(); /* invoking the PANIC() function to stop the system and print a warning message on terminal 0 */





}

