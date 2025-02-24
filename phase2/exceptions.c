 #include "../h/types.h"
 #include "../h/const.h"
 #include "../h/scheduler.h"
 #include "../h/exceptions.h"
 #include "../h/interrupts.h"
 #include "../h/initial.h"
 #include "/usr/include/umps3/umps/libumps.h"
 

int sysCallNum;
/* This variable holds the current time as read from the hardware clock. 
The system uses the STCK (store clock) instruction (or function) to update curr_tod.*/
cpu_t curr_TOD;
 
/**********************************************************************************************
 * updateCurrPcb
 *
 * Update the Current Process' PCB with the saved exception state
 * Use MoveState to copy the saved exception state to the 
 * Current Process' processor state
 * 
 * @param void
 * @return void
**********************************************************************************************/

HIDDEN void updateCurrentPcb(){
    moveState(savedExceptState, &(currrentProcess->p_s) ); 
}

/**********************************************************************************************
 * SYS1 - Create Process
 *
 * This function create a new process. It allocates a new PCB,
 * initializes the new process' state from a1, sets the support
 * struct from a2, sets the new process' time to 0, sets the
 * new process' semaphore address to
 * NULL, inserts the new process as a child of the current
 * process, inserts the new process into the Ready Queue, and
 * increments the process count.
 * 
 * after creating the child, the parent must resume execution 
 * so the system can later choose between the parent and the
 * child (or other ready processes) based on the scheduling policy.
 * 
 * @param stateSYS: the state of the system
 * @param suppStruct: the support struct
 * @return void
*********************************************************************************************/

void createProcess(state_PTR stateSYS, support_t *supportStruct){
    pcb_PTR new_pcb;
    new_pcb = allocPcb(); /* create new PCB for the new process */
    
    if (new_pcb != NULL){  /* allocate PCB success */

        /* update state and set the support struct of new PCB */
        moveState(stateSYS, &(new_pcb->p_s)); 
        new_pcb->p_supportStruct = supportStruct;

        /* The new process has init time (0) and no semaphore (not ready to run yet)*/
        new_pcb->p_time = PROCESS_INIT_START;
        new_pcb->p_semAdd = NULL;

        /* Make this new pcb as the child of the 
        current process and add PVB to ready queue */
        insertChild(currentProcess, new_pcb);
        insertProcQ(&readyQueue, new_pcb);

        /* Increment the process count */
        currentProcess->p_s.s_v0 = SUCCESS_CONST;
        processCount++;
    }
    else{ /* no new PCB for allocation  */

        /* Return error code */
        currentProc->p_s.s_v0 = ERRORCONST; 
    }
    
    /* update the timer for the current process and return control to current process */
    STCK(curr_tod);
    currentProcess->p_time = currentProcess->p_time + (curr_tod - start_tod); 
    
    /* return control to the current process */
    switchContext(currentProcess);
}


/*********************************************************************************************
 * terminateProcess
 * 
 * Process end, execution stops, and its resources are cleaned up, 
 * all of its child processes are also terminated. 
 * This ensures that no part of the process’s “family tree” is left running.
 * 
 * Recursively terminate all children of the process to be terminated.
 * Then determine if the process is blocked on the ASL, in the Ready Queue, or is the Current Process.
 * Remove the terminating process from the parents or ASL or Ready Queue.
 * Free the PCB of the terminating process.
 * Decrement the process count.
 * After all the processes have been terminated, the operating system calls the Scheduler. 
 * This scheduler then selects another process from the ready queue to run, so the system continues operating.
 * 
 * If procSem is not NULL, the process is blocked on an ASL, 
 * so it can remove it from the blocked list.
 * If it is NULL, then the process isn't blocked, 
 * so it must be in the Ready Queue or running.
 * 
 * @param terminate_process: the process to be terminated
 * @return void
/*********************************************************************************************/
HIDDEN void terminateProcess(pcb_PTR terminate_process){ 
    /* The semaphore of the current terminating process*/
    int *this_semaphore; 
    this_semaphore = terminate_process->p_semAdd;

    /* Recursively terminate all childrn */
    while ( !(emptyChild(terminate_process)) ) {
        terminateProcess(removeChild(terminate_process));
    }

    /* After remove all the children, start terminate the current process */
    
    if (terminate_process == currrentProcess) {
        /* If the process to be terminated is the current process, then detach it from the parent to ready to free later */
        outChild(terminate_process);
    } else if (this_semaphore != NULL){ 
        /* If the process is blocked on the ASL, remove it from the blocked list */
        outBlocked(terminate_process);

        /** 
         * For non-device semaphores, the value itself represents how many resources or processes are available so incrementing 
         * it will "release" one waiting process.
         * The soft block count is used to track the overall number of processes blocked on device 
         * operationsindependently from the semaphore’s own value.
        */
        if (!(this_semaphore >= &deviceSemaphores[FIRSTDEVINDEX] && this_semaphore <= &deviceSemaphores[PCLOCKIDX])){ 
            /* If the process is blocked on a non-device semaphore, increment
            the semaphore value to release one waiting process */
            ( *(this_semaphore) )++;
        }
        else {
            softBlockedCount--;
        } 
    } else {
        /* If the process is not blocked, it must be in the Ready Queue */ 
        outProcQ(&readyQueue, terminate_process);
    }

    /* Free the PCB of the terminating process */
    freePcb(terminate_process);
    processCount--;
    terminate_process = NULL;
}

/*********************************************************************************************
 * Passeren
 * 
 * This function is used to request the Nucleus to perform a P operation on a semaphore. 
 * The P operation (or wait) means: If no one doing anything, then let me.
 * If the value of the semaphore is less than 0, the process must be blocked. 
 * If the value of the semaphore is greater than or equal to 0, the process can continue.
 * 
 * @param this_semaphore: the semaphore to be passed
 * @return void
/*********************************************************************************************/
HIDDEN void passeren(int *this_semaphore){
    
    /* Decrement the semaphore value by 1 */
    (*this_semaphore)--;

    if (*this_semaphore < 0) { 
        /* If the value of the semaphore is less than 0, the process must be blocked */
        blockCurr(this_semaphore);
        scheduler();
    }

    /* update the timer for the current process and return control to current process */
    STCK(curr_tod);
    currentProcess->p_time = currentProcess->p_time + (curr_tod - start_tod); 
    
    /* return control to the current process */
    switchContext(currentProcess);
}

/*********************************************************************************************
 * Verhogen
 * 
 * This function is used to request the Nucleus to perform a V operation on a semaphore. 
 * The V operation (or signal) means: I am done, you can go.
 * If the value of the semaphore is less than or equal to 0, the process must be unblocked. 
 * If the value of the semaphore is greater than 0, the process can continue.
 *
 * @param sem: the semaphore to be passed
 * @return void
/*********************************************************************************************/
HIDDEN void verhogen(int *this_semaphore){
    /* Increment the semaphore value by 1 */
    (*this_semaphore)++;

    if (*this_semaphore <= 0) { 
        /* If the value of the semaphore is less than or equal to 0, the process must be unblocked */
        pcb_PTR temp = removeBlocked(sem);
        insertProcQ(&readyQueue, temp);
    }

    /* update the timer for the current process and return control to current process */
    STCK(curr_tod);
    currentProcess->p_time = currentProcess->p_time + (curr_tod - start_tod); 
    
    /* return control to the current process */
    switchContext(currentProcess);
}


/*********************************************************************************************
 * waitForIO
 * 
 * This function transitions the Current Process from the “running” state to a “blocked” state.
 * If the semaphore indicates that the device is busy, the process is blocked until the device becomes available.
 * Then the process is placed on the ASL (This list holds all processes that are waiting for an I/O event)
 * 
 * We need to find the index of the semaphore associated with the device requesting I/O in deviceSemaphores[].
 * The OS has arranged the semaphores in a table or array. 
 * A common way to “calculate” the semaphore is by using an index like:
 * index = (interrupt_line_number - base_line) * number_of_devices_per_line + device_number;
 * (this is because interrupt lines 3 to 7 are used for devices)
 * 
 * The terminal device needs two semaphores to allow independent, concurrent control over its read and write operations.
 * If the wait_for_read is not TRUE, the process is waiting for a terminal read operation, 
 * then we need to increment the index by 8 to reach the write semaphore.
 * 
 *  
 * @param lineNum: the line number of the device
 * @param deviceNum: the device number
 * @param readBool: the read boolean
 * @return void
/*********************************************************************************************/
HIDDEN void waitForIO(int interrupt_line_number, int device_number, int wait_for_read){
    /* The index of the semaphore associated
    with the device requesting I/O in deviceSemaphores[] */
    int semaphore_index; 
    semaphore_index = ((interrupt_line_number - BASE_LINE) * DEVPERINT) + device_number;

    /* If the process is waiting for a terminal read operation, increment the index by 8 to reach the write semaphore */
    if (lineNum == LINE7 && wait_for_read == FALSE){ 
        semaphore_index += DEVPERINT; 
    }

    /* Decrement the semaphore value by 1 */
    (deviceSemaphores[semaphore_index])--;
    softBlockedCount++;

    /* If the value of the semaphore is less than 0, the process must be blocked */
    if (deviceSemaphores[semaphore_index] < 0) { 
        blockCurr(&deviceSemaphores[semaphore_index]);
        scheduler();
    }

    /* update the timer for the current process and return control to current process */
    STCK(curr_tod);
    currentProcess->p_time = currentProcess->p_time + (curr_tod - start_tod);

    /* return control to the current process */
    switchContext(currentProcess);
}


/*********************************************************************************************
 * getCPUTime
 * 
 * This function is used to return the accumulated processor time used by the requesting process.
 * This system call is useful for measuring the CPU usage of a process.
 * 
 * The function places the accumulated processor time used by the requesting process in v0.
 * The function then updates the accumulated CPU time for the Current Process.
 * The function then returns control to the Current Process by loading its (updated) processor state.
 * 
 *          start_tod
 *             │
 *    Process runs...
 *             │
 *         curr_tod (first reading, say 150)
 *             │
 *    -----------------------
 *    | p_time updated:    |  ← p_time += (150 - start_tod)
 *    | s_v0 set to value  |     (CPU time so far)
 *    -----------------------
 *             │   (STCK is called, new time read)
 *             │
 *         new curr_tod (say 155)
 *             │
 *    -----------------------
 *    | p_time updated:    |  ← p_time += (155 - start_tod)
 *    | now reflects extra  |
 *    | time used during   |
 *    | system call call   |
 *    -----------------------
 *             │
 *       Process resumes
 * 
 * @param void
 * @return void
/*********************************************************************************************/
HIDDEN void getCPUTime(){
    /** 
     * Place the accumulated processor time used by the requesting process in v0
     * At the moment the system call is made, the process has been running since a saved start time (start_tod).
     * The variable curr_tod holds the time when the call started.
     * The difference (curr_tod – start_tod) represents the time the process has run since start_tod.
     * This difference is added to the previously accumulated time (p_time) and saved into s_v0. This value is what the calling process will see as its CPU time.
    */
    currentProcess->p_s.s_v0 = currentProcess->p_time + (curr_tod - start_tod); 
    currentProcess->p_time = currentProcess->p_time + (curr_tod - start_tod); 

    /** 
     * update the timer for the current process and return control to current process 
     * Updating the Clock with STCK
     * Right after that, STCK is called. This instruction updates curr_tod with the current time from the hardware clock.
     * There may have been a small delay between the earlier measurement and this new reading. 
    */
    STCK(curr_tod);
    currentProcess->p_time = currentProcess->p_time + (curr_tod - start_tod); 
    
    /* return control to the current process */
    switchContext(currentProcess);
}

/*********************************************************************************************
 * waitForPClock
 * 
 * This function transitions the Current Process from the “running” state to a “blocked” state.
 * Always blocking syscall, since the Pseudo-clock semaphore is a synchronization semaphore.
 * 
 * Decrement the semaphore value by 1.
 * If the value of the semaphore is less than 0, the process must be blocked.
 * If the value of the semaphore is greater
 * than or equal to 0, the process can continue.
 * 
 * @param void
 * @return void
/*********************************************************************************************/

HIDDEN void waitForClock(){
    /* the current process got block for the clock, decrease the semaphore by 1 */
    (deviceSemaphores[CLOCK_INDEX])--;

    /* If the value of the semaphore is less than 0, the process must be blocked */
    if (deviceSemaphores[CLOCK_INDEX] < 0) { 
        blockCurr(&deviceSemaphores[CLOCK_INDEX]);
        scheduler();
    }

    /* update the timer for the current process and return control to current process */
    STCK(curr_tod);
    currentProcess->p_time = currentProcess->p_time + (curr_tod - start_tod);

    /* return control to the current process */
    switchContext(currentProcess);
}

/*********************************************************************************************
 * getSupportData
 * 
 * This function simply return the current process' supportStruct in v0.
 * 
 * The function then updates the accumulated CPU time for the Current Process.
 * The function then returns control to the Current Process by loading its (updated) processor state.
 * 
 * @param void
 * @return void
/*********************************************************************************************/

HIDDEN getSupportData() {
    /* place Current Process' supportStruct in v0 */
    currentProcess->p_s.s_v0 = (int)(currentProcess->p_supportStruct); 
    STCK(curr_tod); 
    currentProcess->p_time = currentProcess->p_time + (curr_tod - start_tod); 
    switchContext(currentProcess); 
}


/*********************************************************************************************
 * checkUserMode
 * 
 * This function checks if a SYSCALL is being executed in user mode.
 * If it is, it sets the Cause.ExcCode to RI (Reserved Instruction)
 * and calls the Program Trap handler.
 * 
 * The status register's user mode bit can be checked using USERPON.
 * If the process was in user mode, we set Cause.ExcCode to RI (10)
 * and handle it as a program trap.
 * 
 * @param savedState: the saved exception state
 * @return void
*********************************************************************************************/
HIDDEN void checkUserMode(state_PTR savedState) {
    if ((savedState->s_status & USERPON) != ALLOFF) {
        /* Set Cause.ExcCode to RI (Reserved Instruction) */
        savedState->s_cause = (savedState->s_cause & ~EXCMASK) | (RESINSTRCODE << CAUSESHFT);
        /* Handle as program trap */
        pgmTrapH();
    }
}



/*********************************************************************************************
*********************************************************************************************/
/*********************************************************************************************
*********************************************************************************************/
/*********************************************************************************************
*********************************************************************************************/
/*********************************************************************************************
*********************************************************************************************/
/*********************************************************************************************
*********************************************************************************************/
/*********************************************************************************************
*********************************************************************************************/
/*********************************************************************************************
*********************************************************************************************/



















/* Function that performs a standard Pass Up or Die operation using the provided index value. If the Current Process' p_supportStruct is
NULL, then the exception is handled as a SYS2; the Current Process and all its progeny are terminated. (This is the "die" portion of "Pass
Up or Die.") On the other hand, if the Current Process' p_supportStruct is not NULL, then the handling of the exception is "passed up." In
this case, the saved exception state from the BIOS Data Page is copied to the correct sup_exceptState field of the Current Process, and 
a LDCXT is performed using the fields from the proper sup_exceptContext field of the Current Process. */
void passUpOrDie(int exceptionCode){ 
    if (currentProc->p_supportStruct != NULL){
        moveState(savedExceptState, &(currentProc->p_supportStruct->sup_exceptState[exceptionCode])); /* copying the saved exception state from the BIOS Data Page directly to the correct sup_exceptState field of the Current Process */
        STCK(curr_tod); /* storing the current value on the Time of Day clock into curr_tod */
        currentProc->p_time = currentProc->p_time + (curr_tod - start_tod); /* updating the accumulated CPU time for the Current Process */
        LDCXT(currentProc->p_supportStruct->sup_exceptContext[exceptionCode].c_stackPtr, currentProc->p_supportStruct->sup_exceptContext[exceptionCode].c_status,
        currentProc->p_supportStruct->sup_exceptContext[exceptionCode].c_pc); /* performing a LDCXT using the fields from the correct sup_exceptContext field of the Current Process */
    }
    else{
        /* the Current Process' p_support_struct is NULL, so we handle it as a SYS2: the Current Process and all its progeny are terminated */
        terminateProcess(currentProc); /* calling the termination function that "kills" the Current Process and all of its children */
        currentProc = NULL; /* setting the Current Process pointer to NULL */
        switchProcess(); /* calling the Scheduler to begin executing the next process */
    }
}

/* Function that represents the entry point into this module when handling SYSCALL events. This function's tasks include, but are not
limited to, incrementing the value of the PC in the stored exception state (to avoid an infinite loop of SYSCALLs), checking to see if an
attempt was made to request a SYSCALL while the system was in user mode (if so, the function handles this case as it would a Program Trap
exception), and checking to see what SYSCALL number was requested so it can invoke an internal helper function to handle that specific
SYSCALL. If an invalid SYSCALL number was provided (i.e., the SYSCALL number requested was nine or above), we invoke the internal
function that performs a standard Pass Up or Die operation using the GENERALEXCEPT index value.  */
void sysTrapH(){
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

/* Function that handles TLB exceptions. The function invokes the internal helper function that performs a standard Pass Up or Die
operation using the PGFAULTEXCEPT index value. */
void tlbTrapH(){
    passUpOrDie(PGFAULTEXCEPT); /* performing a standard Pass Up or Die operation using the PGFAULTEXCEPT index value */
}

/* Function that handles Program Trap exceptions. The function invokes the internal helper function that performs a standard Pass Up or Die
operation using the GENERALEXCEPT index value. */
void pgmTrapH(){
    passUpOrDie(GENERALEXCEPT); /* performing a standard Pass Up or Die operation using the GENERALEXCEPT index value */
}


/* Function that handles the steps needed for blocking a process. The function updates the accumulated CPU time 
for the Current Process, and blocks the Current Process on the ASL. */
void blockCurr(int *sem){
    STCK(curr_tod); /* storing the current value on the Time of Day clock into curr_tod */
    currentProc->p_time = currentProc->p_time + (curr_tod - start_tod); /* updating the accumulated CPU time for the Current Process */
    insertBlocked(sem, currentProc); /* blocking the Current Process on the ASL */
    currentProc = NULL; /* setting currentProc to NULL because the old process is now blocked */ 
}
