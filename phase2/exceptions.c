
/*****
 * 
 * Among the 8 syscalls, every syscall except SYS2 (terminateProcess) is designed to return control to the current process. In SYS2 the process is terminated, so control isn’t returned to the original process but instead to a newly scheduled one. All the other syscalls (SYS1, SYS3, SYS4, SYS5, SYS6, SYS7, SYS8) update the process's CPU time and then call the function (switchContext) that resumes the executing process.
 * 
 */




#include "../h/types.h"
#include "../h/const.h"
#include "../h/initial.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../h/scheduler.h"
#include "../h/pcb.h"
#include "../h/asl.h"
#include "/usr/include/umps3/umps/libumps.h"
 
/* ---------------------------------------------------------------------------------------------- */
/* ------------------------------------------ Variables ----------------------------------------- */
/* ---------------------------------------------------------------------------------------------- */

int sysCallNum;
cpu_t curr_TOD; /* Record the time of the current process */
 

/* ---------------------------------------------------------------------------------------------- */
/* ------------------------------------------ SYSCALL ------------------------------------------- */
/* ---------------------------------------------------------------------------------------------- */


/**********************************************************************************************
 * SYS1 - Create Process
 *
 * @brief
 * This function create a new process. This new process is a child of the current process.
 * The process requesting the SYS1 service continues to exist and to execute (Pandos page 25)
 * If new process cannot be created due to lack of resources
 * an error code of -1 is placed/returned in the caller’s v0, otherwise, 
 * return the value 0 in the caller’s v0.
 * 
 * @protocol
 * 1. Seting up new process 
 *      - It allocates a new PCB,                                       (S1)
 *      - initializes the new process' state from a1,                   (S2)
 *      - p_supportStruct from a2,                                      (S3)
 *      - sets the new process' time to 0,                              (S4)
 *      - sets the new process' semaphore address to NULL,              (S5)
 *      - inserts the new process as a child of the current process     (S6)
 *      - inserts the new process into the Ready Queue                  (S7)
 *      - increments the process count.                                 (S8)
 * 2. If the allocation of the new PCB was not successful,
 *     - Place the value -1 in the caller’s v0.                         (S9)
 * 3. Return control to the current process.                            (S10)
 * 
 * 
 * @param state_process: the state of the system
 * @param support_process: the support struct
 * @return void
*********************************************************************************************/

void createProcess(state_PTR state_process, support_PTR support_process) {

    /* S1: It allocates a new PCB */
    pcb_PTR new_pcb = allocPcb();
    
    /* S1: If the allocation of the new PCB was successful */
    if (new_pcb != NULL){ 

        /* S2: initializes the new process' state from a1 */
        moveState(state_process, &(new_pcb->p_s)); 

        /* S3: p_supportStruct from a2 */
        new_pcb->p_supportStruct = support_process; 

        /* S4: The new process has init time (0) and no semaphore (not ready to run yet)*/
        new_pcb->p_time = PROCESS_INIT_START;

        /* S5: sets the new process' semaphore address to NULL */
        new_pcb->p_semAdd = NULL;

        /* S6 + S7 Make this new pcb as the child of the 
        current process and add PVB to ready queue */
        insertChild(currentProcess, new_pcb);
        insertProcQ(&readyQueue, new_pcb);

        /* Place the value 0 in the caller’s v0. */
        currentProcess->p_s.s_v0 = SUCCESS_CONST;

        /* S8: increments the process count. */
        processCount++;

    } else{ /* S9: no new PCB for allocation  */
        
        /* Place the value -1 in the caller’s v0. */
        currentProcess->p_s.s_v0 = ERRORCONST; 
    }
    
    /* update the timer for the current process and return control to current process */
    STCK(curr_TOD);
    currentProcess->p_time = currentProcess->p_time + (curr_TOD - start_TOD); 
    
    /* S10: return control to the current process */
    switchContext(currentProcess);
}

/*********************************************************************************************
 * SYS2 - terminateProcess
 * 
 * @brief
 * Process end, execution stops, and its resources are cleaned up, all of its child processes are also terminated. 
 * This ensures that no part of the process’s “family tree” is left running.
 * 
 * @protocol
 * 1. Recursively terminate all children of the process to be terminated.
 * 2. Check the position of the process:
 *      - Then determine if the process is blocked on the ASL,
 *          - OutBlocked the process
 *          - If it is in device semaphores, increment the semaphore value
 *          - if it is in non-device semaphores, decrease the soft block count
 *      - In the Ready Queue
 *          - remove it from the Ready Queue to free it later
 *      - Current Process
 *          - then simply detach it from the parent to ready to free later
 * 3. After all the processes have been terminated, the operating system calls the Scheduler, 
 * this scheduler then selects another process from the ready queue to run, so the system continues operating.
 * 
 * 
 * @note
 * With non‐device semaphores, the semaphore value directly represents available instances of a resource, 
 * so a V operation can simply increment that value to "release" a waiting process.
 * 
 * However, device semaphores are used to synchronize with asynchronous I/O events. 
 * Their values aren’t meant to count available items but to indicate that an I/O operation
 * is in progress or has completed. Instead of adjusting the semaphore value, 
 * the OS maintains a separate count (softBlockedCount) to track how many processes are waiting for a device operation to finish.
 * 
 * Incrementing the device semaphore directly might disrupt the intended signaling behavior for device I/O.
 * 
 * @param terminate_process: the process to be terminated
 * @return void
/*********************************************************************************************/

HIDDEN void terminateProcess(pcb_PTR terminate_process){ 

    /* Step 1: Recursively terminate all childrn */
    while ( !(emptyChild(terminate_process)) ) {
        terminateProcess(removeChild(terminate_process));
    }

    /* Step 2: Check the position of the process using semaphore */
    int *this_semaphore = terminate_process->p_semAdd;

    /* If the process to be terminated is the current process */
    if (terminate_process == currentProcess) {
        
        /* Then make it orphan by remove it from the parent to ready to free later */
        outChild(terminate_process);

    } else if (this_semaphore != NULL){ /* If the process is blocked on the ASL */
        
        /* Remove it from the blocked list */
        outBlocked(terminate_process);

        /* If the process is in non-device semaphores, increament the semaphore */
        if ( ! (
                (this_semaphore >= &deviceSemaphores[0]) && 
                (this_semaphore <= &deviceSemaphores[CLOCK_INDEX])
            )) { 
                    ( *(this_semaphore) )++;
        }
        else {

            /* If the process is in device semaphores, decrease the soft block */
            softBlockedCount--;
        } 
    } else {

        /* If the process is not blocked, it must be in the Ready Queue */ 
        outProcQ(&readyQueue, terminate_process);
    }

    /* STEP 3; Free the PCB of the terminating process */
    freePcb(terminate_process);
    processCount--;
    terminate_process = NULL;
}

/*********************************************************************************************
 * SYS 3 - Passeren
 * 
 * @brief
 * This function is used to request the Nucleus to perform a P operation on a semaphore. 
 * The P operation (or wait) means: If no one doing anything, then let me.
 * If the value of the semaphore is less than 0, the process must be blocked. 
 * If the value of the semaphore is greater than or equal to 0, the process can continue.
 * 
 * @protocol
 * 1. Decrement the semaphore value by 1
 * 2. If the value of the semaphore is less than 0, the process must be blocked
 * 3. update the timer for the current process and return control to current process
 * 4. return control to the current process
 * 
 * @param this_semaphore: the semaphore to be passed
 * @return void
/*********************************************************************************************/

HIDDEN void passeren(int *this_semaphore){
    
    /* Decrement the semaphore value by 1 */
    (*this_semaphore)--;

    if (*this_semaphore < 0) { 
        /* If the value of the semaphore is less than 0, the process must be blocked */
        blockCurrentProcessHelper(this_semaphore);
        scheduler();
    }

    /* update the timer for the current process and return control to current process */
    STCK(curr_TOD);
    updateProcessTimeHelper(currentProcess, start_TOD, curr_TOD);
    
    /* return control to the current process */
    switchContext(currentProcess);
}

/*********************************************************************************************
 * SYS4 - Verhogen
 * 
 * @brief
 * This function is used to request the Nucleus to perform a V operation on a semaphore. 
 * The V operation (or signal) means: I am done, you can go.
 * If the value of the semaphore is less than or equal to 0, the process must be unblocked. 
 * If the value of the semaphore is greater than 0, the process can continue.
 *
 * @protocol
 * 1. Increment the semaphore value by 1
 * 2. If the value of the semaphore is less than or equal to 0, the process must be unblocked
 * 3. update the timer for the current process and return control to current process
 * 4. return control to the current process
 * 
 * @param this_semaphore: the semaphore to be passed
 * @return void
**********************************************************************************************/

HIDDEN void verhogen(int *this_semaphore){
    /* Increment the semaphore value by 1 */
    (*this_semaphore)++;

    if (*this_semaphore <= 0) { 
        /* If the value of the semaphore is less than or equal to 0, the process must be unblocked */
        pcb_PTR temp = removeBlocked(this_semaphore);
        insertProcQ(&readyQueue, temp);
    }

    /* update the timer for the current process and return control to current process */
    STCK(curr_TOD);
    updateProcessTimeHelper(currentProcess, start_TOD, curr_TOD);
    
    /* return control to the current process */
    switchContext(currentProcess);
}

/*********************************************************************************************
 * SYS5 - waitForIO
 * 
 * @brief
 * This function transitions the Current Process from the “running” state to a “blocked” state.
 * It is to block the current process when it initiates a synchronous I/O operation.
 * 
 * @protocol
 * 1. Find the index of the semaphore associated with the device requesting I/O in deviceSemaphores[].
 * 2. check if the process is waiting for a terminal read operation or write operation
 * 3. Decrement the semaphore value by 1 and increment the soft block count
 * 4. If the value of the semaphore is less than 0, the process must be blocked
 * 5. update the timer for the current process and return control to current process
 * 
 * 
 * @note
 * IMPORTANT:
 * Since the semaphore that will have a P operation performed on it is a synchronization semaphore, 
 * this call should always block the Current Process on the ASL, after which the Scheduler is called.
 * Therefore, we expect that step 5 is NOT execute. However, for the best coding practice, I implemented
 * an "if" statement to check on the value of the semaphore.
 * 
 * @note
 * - A common way to “calculate” the semaphore is by using an index like:
 * index = (interrupt_line_number - base_line) * number_of_devices_per_line + device_number;
 * (this is because interrupt lines 3 to 7 are used for devices)
 * 
 * - The terminal device needs two semaphores to allow independent, concurrent control over its read and write operations.
 * If the wait_for_read is not TRUE, the process is waiting for a terminal read operation, 
 * then we need to increment the index by 8 to reach the write semaphore.
 * 
 * @param interrupt_line_number: the line number of the device
 * @param device_number: the device number
 * @param wait_for_read: the read boolean
 * @return void
/*********************************************************************************************/

HIDDEN void waitForIO(int interrupt_line_number, int device_number, int wait_for_read){

    /* Step 1: Find the index of the semaphore */
    int semaphore_index = ((interrupt_line_number - BASE_LINE) * DEVPERINT) + device_number;

    /* Step 2: check on read or write operation */
    if (interrupt_line_number == LINE7 && wait_for_read == FALSE){ 
        semaphore_index += DEVPERINT; 
    }

    /* Step 3: Decrease semaphore and increase soft block count */
    (deviceSemaphores[semaphore_index])--;
    softBlockedCount++;

    /* Step 4: If the value of the semaphore is less than 0, the process must be blocked */
    if (deviceSemaphores[semaphore_index] < 0) { 
        blockCurrentProcessHelper(&deviceSemaphores[semaphore_index]);
        scheduler();
    }

    /* Step 5: update the timer for the current process and return control to current process */
    STCK(curr_TOD);
    updateProcessTimeHelper(currentProcess, start_TOD, curr_TOD);

    /* return control to the current process */
    switchContext(currentProcess);
}

/*********************************************************************************************
 * getCPUTime
 * 
 * @brief
 * This function is used to return the accumulated processor time used by the requesting process.
 * This system call is useful for measuring the CPU usage of a process.
 * 
 * @protocol
 * 1. The function places the accumulated processor time used by the requesting process in v0.
 * 3. The function then add the syscall CPU time to the current process time
 * 4. The function then returns control to the Current Process by loading its (updated) processor state. 
 * 
 * @note
 *          start_tod
 *             │
 *    Process runs...
 *             │
 *         curr_tod (first reading, say 150)
 *             │
 *    -----------------------
 *    | p_time updated:     |  ← p_time += (150 - start_tod)
 *    | s_v0 set to value   |     (CPU time so far)
 *    -----------------------
 *             │   (STCK is called, new time read)
 *             │
 *         new curr_tod (say 155)
 *             │
 *    -----------------------
 *    | p_time updated:     |  ← p_time += (155 - start_tod)
 *    | now reflects extra  |
 *    | time used during    |
 *    | system call call    |
 *    -----------------------
 *             │
 *       Process resumes
 * 
 * @param void
 * @return void
/*********************************************************************************************/

HIDDEN void getCPUTime(){

    /* Step 1: The function places the accumulated processor time used by the requesting process in v0 */
    STCK(curr_TOD);
    updateProcessTimeHelper(currentProcess, start_TOD, curr_TOD);
    currentProcess->p_s.s_v0 = currentProcess->p_time;

    /* Step 2: update the syscall CPU time for this process */
    STCK(curr_TOD);
    updateProcessTimeHelper(currentProcess, start_TOD, curr_TOD);

    /* Step 3: return control to the current process */
    switchContext(currentProcess);
}

/*********************************************************************************************
 * SYS7 - waitForPClock
 * 
 * @brief
 * This service performs a P operation on the Nucleus maintained Pseudo-cloc semaphore. 
 * This semaphore is V’ed every 100 milliseconds by the Nucleus (Pandos page 29)
 * This function transitions the Current Process from the “running” state to a “blocked” state.
 * Always blocking syscall, since the Pseudo-clock semaphore is a synchronization semaphore.
 * 
 * @protocol
 * 1. Decrement the semaphore value by 1.
 * 2. If the value of the semaphore is less than 0, the process must be blocked.
 * 3. If the value of the semaphore is greater than or equal to 0, the process can continue.
 * 
 * @note
 * IMPORTANT:
 * The Pseudo-clock semaphore is a synchronization semaphore, so this call should ALWAYS 
 * block the Current Process on the ASL, after which the Scheduler is called.
 * Therefore, we expect that step 3 is NOT execute. However, for the best coding practice, I implemented
 * an "if" statement to check on the value of the semaphore.
 * 
 * @param void
 * @return void
/*********************************************************************************************/

HIDDEN void waitForClock(){
    /* STEP 1: the current process got block for the clock, decrease the semaphore by 1 */
    (deviceSemaphores[CLOCK_INDEX])--;

    /* STEP 2: If the value of the semaphore is less than 0, the process must be blocked */
    if (deviceSemaphores[CLOCK_INDEX] < 0) { 
        blockCurrentProcessHelper(&deviceSemaphores[CLOCK_INDEX]);
        scheduler();
    }

    /* STEP 3: update the timer for the current process and return control to current process */
    STCK(curr_TOD);
    updateCurrentProcessTimeHelper(currentProcess, start_TOD, curr_TOD);

    /* return control to the current process */
    switchContext(currentProcess);
}

/*********************************************************************************************
 * getSupportData
 * 
 * @brief
 * This function simply return the current process' supportStruct in v0.
 * (FINALLY THERE IS A NORMAL FUNCTIION......... THANKS GOD)
 * 
 * @protocol
 * 1. The function places the current process' supportStruct in v0.
 * 2. The function then add the syscall CPU time to the current process time
 * 
 * @param void
 * @return void
/*********************************************************************************************/

HIDDEN getSupportData() {
    /* Step 1: The function place the support Struct in v0 */
    currentProcess->p_s.s_v0 = (int)(currentProcess->p_supportStruct); 

    /* Step 2: update the syscall CPU time for this process */
    STCK(curr_TOD); 
    updateProcessTimeHelper(currentProcess, start_TOD, curr_TOD);
    switchContext(currentProcess); 
}


/* ---------------------------------------------------------------------------------------------- */
/* ------------------------------------- HELPER + HANDLER --------------------------------------- */
/* ---------------------------------------------------------------------------------------------- */

/*********************************************************************************************
 * PassUporDie
 * 
 * @brief
 * When a process triggers an exception that is not one of the basic system calls (SYS1–SYS8), 
 * the Nucleus must decide how to handle it. 
 * The decision depends on whether the process was set up to deal with exceptions 
 * 
 * - Pass Up: If the process was created with a valid support structure, the Nucleus “passes up” the exception. 
 * That means it hands off the exception to a higher-level handler in the Support Level, 
 * which is designed to handle more complex exceptions.
 * 
 * - Die: If the process did not provide a support structure, then the exception is fatal. 
 * The Nucleus will terminate (kill) the process and all its child processes. This is the “die” part.
 * 
 * @protocol
 * 1. If the current process has a support structure, (Pandos page 36 - 37)
 *      - pass up the exception
 *      - perform a LDCXT using the fields from the correct sup_exceptContext field of the Current Process
 * 2. If the current process does not have a support structure, we activate SYS2, which include:
 *      - terminate the process and all its progeny
 *      = set the Current Process pointer to NULL
 *      - call the Scheduler to begin executing the next process
 * 
 * @note
 * Not all exceptions are simple errors. Some, like page faults, might be recoverable. 
 * Processes that are designed to handle such exceptions are created with a support structure. 
 * The “pass up” mechanism hands the exception to these handlers so the process can attempt recovery rather than just crashing.
 * 
 * For processes that are not set up to handle exceptions, the OS chooses to terminate them. 
 * This prevents processes that are not designed to cope with errors from causing unpredictable behavior or corrupting system data.
 * 
 * @param exceptionCode: the exception code 
*********************************************************************************************/

HIDDEN void passUpOrDie(int exceptionCode){ 

    if (currentProcess->p_supportStruct != NULL){

        /* Step 1.1: If the current process has a support structure, pass up the exception */
        moveState(savedExceptState, &(currentProcess->p_supportStruct->sup_exceptState[exceptionCode]));
        
        STCK(curr_TOD);
        updateProcessTimeHelper(currentProcess, start_TOD, curr_TOD);

        /* Step 1.2: perform a LDCXT using the fields from the correct sup_exceptContext field of the Current Process */
        LDCXT(
            currentProcess->p_supportStruct->sup_exceptContext[exceptionCode].c_stackPtr, 
            currentProcess->p_supportStruct->sup_exceptContext[exceptionCode].c_status,
            currentProcess->p_supportStruct->sup_exceptContext[exceptionCode].c_pc);
    }
    else{
        /* Call the SYS2 */
        sysCallNum = SYS2_NUM;
        systemTrapHandler();
    }
}









/*********************************************************************************************
 * blockCurrentProcessHelper
 * 
 * @brief
 * This function is used to block the current process.
 * The function takes the semaphore as an argument.
 * 
 * @protocol
 * 1. update the timer for the current process
 * 2. insert the current process into the blocked list
 * 3. set the current process to NULL
 * 
 * @note
 * Indicating that no process is currently being handled after an exception
 * If the current process is being terminated due to an exception, 
 * setting the pointer to NULL ensures that no other part of the system 
 * accidentally tries to access or modify a process that's no longer valid.
 * 
 * @param this_semaphore: the semaphore to be passed
 * @return void
/*********************************************************************************************/

HIDDEN void blockCurrentProcessHelper(int *this_semaphore){
	STCK(curr_TOD);
	currentProcess->p_time = currentProcess->p_time + (curr_tod - start_tod);
    updateCurrentProcessTimeHelper(currentProcess, start_TOD, curr_TOD);
	
    insertBlocked(this_semaphore, currentProcess);
    currentProcess = NULL;
}

/*********************************************************************************************
 * addPigeonCurrentProcessHelper
 * 
 * @brief
 * this function is used to add the exception state to the current process.
 * when the current process comeback with the state, notify the OS with the exception state
 * 
 * @protocol
 * 1. add the exception state to the current process
 * 
 * @note
 * This function is used in the interrupts.c to add the exception state to the current process.
 * so when the current process comeback with the state, notify the OS with the exception state that 
 * the interrupt has been handled.
 * 
 * I feel like the process is like a pigeon, 
 * and the exception state is like a message that the pigeon carries.
 * 
 * @param void
 * @return void
/*********************************************************************************************/

HIDDEN void addPigeonCurrentProcessHelper(){
    moveState(savedExceptState, &(currentProcess->p_s) ); 
}










/*********************************************************************************************
 * programTrapHandler + tlbTrapHandler
 * 
 * @brief
 * 
 * 
 * 
 * @protocol
 * 
 * 
 * 
 * 
 * @param void
 * @return void
*********************************************************************************************/
void sysCallExceptHandler(){
    passUpOrDie(GENERALEXCEPT); 
}

void tlbTrapHandler(){
    passUpOrDie(PGFAULTEXCEPT); 
}

void programTrapHandler(){
    passUpOrDie(GENERALEXCEPT);
}











/*********************************************************************************************
 * checkUserMode
 * 
 * @brief
 * This function is used to check if the current process is in user mode.
 * If the current process is in user mode, then it will set the Cause.ExcCode to RI (Reserved Instruction)
 * and handle it as a program trap.
 * 
 * @protocol
 * 1. Check if the current process is in user mode
 * 2. If the current process is in user mode, then set the Cause.ExcCode to RI (Reserved Instruction)
 * 3. Handle it as a program trap
 * 
 * @param savedState: the saved state of the current process
 * @return void
*********************************************************************************************/

HIDDEN void checkUserMode(state_PTR current_state) {
    if ((current_state->s_status & USERPON) != ALLOFF) {
        
        current_state->s_cause = (current_state->s_cause & ~EXCMASK) | (RESINSTRCODE << CAUSESHFT);
        
        pgmTrapH();
    }
}




/* Function that represents the entry point into this module when handling SYSCALL events. This function's tasks include, but are not
limited to, incrementing the value of the PC in the stored exception state (to avoid an infinite loop of SYSCALLs), checking to see if an
attempt was made to request a SYSCALL while the system was in user mode (if so, the function handles this case as it would a Program Trap
exception), and checking to see what SYSCALL number was requested so it can invoke an internal helper function to handle that specific
SYSCALL. If an invalid SYSCALL number was provided (i.e., the SYSCALL number requested was nine or above), we invoke the internal
function that performs a standard Pass Up or Die operation using the GENERALEXCEPT index value.  */
void systemTrapHandler(){
    /* initializing variables that are global to this module, as well as savedExceptState */ 
    savedExceptState = (state_PTR) BIOSDATAPAGE; /* initializing the saved exception state to the state stored at the start of the BIOS Data Page */
    sysNum = savedExceptState->s_a0; /* initializing the SYSCALL number variable to the correct number for the exception */

    savedExceptState->s_pc = savedExceptState->s_pc + WORDLEN;

    /* Perform checks to make sure we want to proceed with handling the SYSCALL (as opposed to pgmTrapH) */
    if (((savedExceptState->s_status) & USERPON) != ALLOFF){ /* if the process was executing in user mode when the SYSCALL was requested */
        savedExceptState->s_cause = (savedExceptState->s_cause) & RESINSTRCODE; /* setting the Cause.ExcCode bits in the stored exception state to RI (10) */
        pgmTrapH(); /* invoking the internal function that handles program trap events */
    }
    
    if ((sysNum<SYS1NUM) || (sysNum > SYS8NUM)){ /* check if the SYSCALL number was not 1-8 (we'll punt & avoid uniquely handling it) */
        pgmTrapH(); /* invoking the internal function that handles program trap events */
    } 
    
    updateCurrPcb(currentProc); /* copying the saved processor state into the Current Process' pcb  */
    
    /* enumerating the sysNum values (1-8) and passing control to the respective function to handle it */
    switch (sysNum){ 
        case SYS1NUM: /* if the sysNum indicates a SYS1 event */
            /* a1 should contain the processor state associated with the SYSCALL */
            /* a2 should contain the (optional) support struct, which may be NULL */
            createProcess((state_PTR) (currentProc->p_s.s_a1), (support_t *) (currentProc->p_s.s_a2)); /* invoking the internal function that handles SYS1 events */

        case SYS2NUM: /* if the sysNum indicates a SYS2 event */
            terminateProcess(currentProc); /* invoking the internal function that handles SYS2 events */
            currentProc = NULL; /* setting the Current Process pointer to NULL */
            switchProcess(); /* calling the Scheduler to begin executing the next process */
        
        case SYS3NUM: /* if the sysNum indicates a SYS3 event */
            /* a1 should contain the addr of semaphore to be P'ed */
            waitOp((int *) (currentProc->p_s.s_a1)); /* invoking the internal function that handles SYS3 events */
        
        case SYS4NUM: /* if the sysNum indicates a SYS4 event */
            /* a1 should contain the addr of semaphore to be V'ed */
            signalOp((int *) (currentProc->p_s.s_a1)); /* invoking the internal function that handles SYS4 events */

        case SYS5NUM: /* if the sysNum indicates a SYS5 event */
            /* a1 should contain the interrupt line number of the interrupt at the time of the SYSCALL */ 
            /* a2 should contain the device number associated with the specified interrupt line */
            /* a3 should contain TRUE or FALSE, indicating if waiting for a terminal read operation */
            waitForIO(currentProc->p_s.s_a1, currentProc->p_s.s_a2, currentProc->p_s.s_a3); /* invoking the internal function that handles SYS5 events */

        case SYS6NUM: /* if the sysNum indicates a SYS6 event */
            getCPUTime(); /* invoking the internal function that handles SYS6 events */
        
        case SYS7NUM: /* if the sysNum indicates a SYS7 event */
            waitForPClock(); /* invoking the internal function that handles SYS 7 events */
        
        case SYS8NUM: /* if the sysNum indicates a SYS8 event */
            getSupportData(); /* invoking the internal function that handles SYS 8 events */
        
    }
}



