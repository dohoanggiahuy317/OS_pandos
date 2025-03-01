
/**********************************************************************************************
 * exceptions.c
 * 
 * @brief
 * 1. Exception Handler
 * This file provides the core implementation for handling system call exceptions
 * in our operating system design. It handle different types of exceptions based on the register cause ExeCode.
 * It direct the control to the appropriate handler function for each exception type.
 * In particular, it can branch to the 
 *      interruptTrapHandler, 
 *      tlbTrapHandler, 
 *      programTrapHandler,
 *      and systemTrapHandler functions based on the exception code.
 * 
 * 2. System Calls
 * This file is also responsible for managing the system calls and their associated behaviors.
 * It manages 8 distinct system calls, including createProcess, terminateProcess, passeren, verhogen,
 * waitForIO, getCPUTime, waitForClock, and getSupportData.
 * 
 * 3. Different Types of Exceptions
 *      - interruptTrapHandler: function is the handler for Interrupt exceptions. 
 *          It manages the 7 device interrupts, but it is NOT implemented in this file.
 *      - tlbTrapHandler function is the handler for TLB exceptions 
 *      - programTrapHandler: function is the handler for Program Trap exceptions
 *      - systemTrapHandler: function is the handler for SYSCALL exceptions. It manages the 8 system calls
 * 
 * The file also create several "HELPER" handler functions for different types of exceptions, including 
 *      - userModeTrapHandler: simulates a Program Trap exception when a privileged service is requested in user-mode
 *      - sysCallOutRangeHandler: handles in appropriate SYSCALL exceptions
 * 
 * 4. PassUpOrDie
 * The passUpOrDie function is used to handle exceptions that are not basic system calls (SYS1-SYS8).
 * It determines whether the process was set up to handle exceptions and either passes the exception
 * to a higher-level handler or terminates the process and all its child processes.
 * This is used to support exception handler above
 * 
 * 5. HELPER FUNCTIONS
 * The file also provides several helper functions to support the system calls and exception handling.
 * These include
 *     - addPigeonCurrentProcessHelper: add the exception state to the current process
 *      - blockCurrentProcessHelper: block the current process
 *
 * @def
 * - System Call Handling (systemTrapHandler): The file distinguishes between system calls that return control
 * to the original process and SYS2 (terminateProcess) which terminates the process.
 * 
 * - Process Management: While most system calls update the process's CPU time and then
 * resume the current process, the SYS2 system call terminates the process and invokes a
 * context switch to schedule another process.
 * 
 * @note
 * @attention
 * IMPORTANT:
 * Among the 8 syscalls, every syscall except SYS2 (terminateProcess) is designed to
 *  return control to the current process. In SYS2 the process is terminated, 
 * so control isn’t returned to the original process but instead to a newly scheduled one. 
 * All the other syscalls (SYS1, SYS3, SYS4, SYS5, SYS6, SYS7, SYS8) update the process's CPU 
 * time and then call the function (switchContext) that resumes the executing process.
 * 
 * For SYSCALLs calls that do not block or terminate, control is returned to the
 * Current Process at the conclusion of the Nucleus’s SYSCALL exception handler.
 *
 * Also, I set the SYSCALL function to be hidden, so it is only visible within this file.
 *
 * @remark
 * This file is essential in defining the operational semantics of system calls in the
 * operating system. It outlines the behavior of each call, especially focusing on the
 * control flow between processes, and demonstrates how the system distinguishes between
 * calls that continue execution and the one that ends the process.
 * 
 * The file encapsulates the low-level mechanism by which the operating system
 * mediates process execution, time accounting, and context switching, supporting the broader
 * scheduling and process management subsystems.
 * 
 * @author
 * JaWeee Do
**********************************************************************************************/


#include "../h/initial.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/asl.h"
#include "../h/pcb.h"
#include "/usr/include/umps3/umps/libumps.h"
 
/* ---------------------------------------------------------------------------------------------- */
/* ------------------------------------------ Variables ----------------------------------------- */
/* ---------------------------------------------------------------------------------------------- */

/* The system call number */
int sysCallNum;

/* ---------------------------------------------------------------------------------------------- */
/* --------------------------------------- HELPER FUNCS ----------------------------------------- */
/* ---------------------------------------------------------------------------------------------- */

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
*********************************************************************************************/
void addPigeonCurrentProcessHelper() {
    moveStateHelper(savedExceptionState, &(currentProcess->p_s) ); 
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
 * After that, the program might want to invoke scheduler to select another process to run (which
 * is not implement in this fuction but a REMINDER to set it up)
 * 
 * @param this_semaphore: the semaphore to be passed
 * @return void
*********************************************************************************************/
void blockCurrentProcessHelper(int *this_semaphore){

    /* STEP 1: update the timer for the current process */
    STCK(curr_TOD);
    updateProcessTimeHelper(currentProcess, start_TOD, curr_TOD);

    /* STEP 2: insert the current process into the blocked list */
    insertBlocked(this_semaphore, currentProcess);

    /* STEP 3: set the current process to NULL */
    currentProcess = NULL;
}

/* ---------------------------------------------------------------------------------------------- */
/* -------------------------------------- EXCEPTION HANDLER ------------------------------------- */
/* ---------------------------------------------------------------------------------------------- */

/*********************************************************************************************
 * exceptionHandler
 * 
 * @brief
 * This function is used to handle exceptions. 
 * It is the entry point for all exceptions.
 * The cause of this exception is encoded in the .ExcCode field of the Cause register
 * (Cause.ExcCode) in the saved exception state. [Section 3.3-pops]
 *      - For exception code 0 (Interrupts), processing should be passed along to your
 *          Nucleus’s device interrupt handler. [Section 3.6]
 *      - For exception codes 1-3 (TLB exceptions), processing should be passed
 *          along to your Nucleus’s TLB exception handler. [Section 3.7.3]
 *      - For exception codes 4-7, 9-12 (Program Traps), processing should be passed
 *          along to your Nucleus’s Program Trap exception handler. [Section 3.7.2]
 *      - For exception code 8 (SYSCALL), processing should be passed along to
 *          your Nucleus’s SYSCALL exception handler. [Section 3.5]
 * (Pandos page 24)
 * 
 * @protocol
 * 1. Get the execution code from the cause register
 * 2. Check the execution code and call the appropriate handler
 * 
 * @param void
 * @return void
***********************************************************************************************/
void exceptionHandler() {

    /* STEP 1: Get the execution code from the cause register */
    state_PTR savedState = (state_PTR) BIOSDATAPAGE;
    int execCode = ((savedState->s_cause) & EXC_CODE_MASK) >> EXC_CODE_SHIFT;

    /* STEP 2: Check the execution code and call the appropriate handler */
    switch (execCode) {
        case 0:
            interruptTrapHandler();
        case 1:    
        case 2:    
        case 3:  
            tlbTrapHandler();
        case 8:   
            systemTrapHandler();
        case 4:  
        case 5:  
        case 6: 
        case 7:    
        case 9: 
        case 10:   
        case 11:    
        case 12:   
            programTrapHandler();
    }
}


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
HIDDEN void createProcess(state_PTR state_process, support_PTR support_process) {

    /* S1: It allocates a new PCB */
    pcb_PTR new_pcb = allocPcb();
    
    /* S1: If the allocation of the new PCB was successful */
    if (new_pcb != NULL){ 

        /* S2: initializes the new process' state from a1 */
        moveStateHelper(state_process, &(new_pcb->p_s)); 

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
        currentProcess->p_s.s_v0 = ERROR_CONST; 
    }
    
    /* update the timer for the current process and return control to current process */
    STCK(curr_TOD);
    updateProcessTimeHelper(currentProcess, start_TOD, curr_TOD);
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
*********************************************************************************************/
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
                (this_semaphore >= &semaphoreDevices[0]) && 
                (this_semaphore <= &semaphoreDevices[CLOCK_INDEX])
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
*********************************************************************************************/
HIDDEN void passeren(int *this_semaphore){
    
    /* Decrement the semaphore value by 1 */
    (*this_semaphore)--;

    debugExceptionHandler(8, *this_semaphore, 0, 0); /* I WILL NOT DELETE THIS TO MEMORIZE AND REMARK IT AS ONE OF THE 4-HOUR DEBUGGING */

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

    debugExceptionHandler(9, *this_semaphore, 0, 0); /* I WILL NOT DELETE THIS TO MEMORIZE AND REMARK IT AS ONE OF THE 4-HOUR DEBUGGING */

    if (*this_semaphore <= 0) { 
        /* If the value of the semaphore is less than or equal to 0, the process must be unblocked */
        pcb_PTR this_pcb = removeBlocked(this_semaphore);

        if (this_pcb != NULL) {
            insertProcQ(&readyQueue, this_pcb);
        }
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
 * 1. Find the index of the semaphore associated with the device requesting I/O in semaphoreDevices[].
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
*********************************************************************************************/
HIDDEN void waitForIO(int interrupt_line_number, int device_number, int wait_for_read){

    /* Step 1: Find the index of the semaphore */
    int semaphore_index = ((interrupt_line_number - BASE_LINE) * DEVPERINT) + device_number;

    /* Step 2: check on read or write operation */
    if (interrupt_line_number == LINE7 && wait_for_read != TRUE){ 
        semaphore_index += DEVPERINT; 
    }

    /* Step 3: Decrease semaphore and increase soft block count */
    softBlockedCount++;
    (semaphoreDevices[semaphore_index])--;
    

    /* Step 4: If the value of the semaphore is less than 0, the process must be blocked */
    if (semaphoreDevices[semaphore_index] < 0) { 
        blockCurrentProcessHelper(&semaphoreDevices[semaphore_index]);
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
*********************************************************************************************/
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
*********************************************************************************************/
HIDDEN void waitForClock(){
    /* STEP 1: the current process got block for the clock, decrease the semaphore by 1 */
    (semaphoreDevices[CLOCK_INDEX])--;

    /* STEP 2: If the value of the semaphore is less than 0, the process must be blocked */
    if (semaphoreDevices[CLOCK_INDEX] < 0) { 
        softBlockedCount++;
        blockCurrentProcessHelper(&semaphoreDevices[CLOCK_INDEX]);
        scheduler();
    }

    /* STEP 3: update the timer for the current process and return control to current process */
    STCK(curr_TOD);
    updateProcessTimeHelper(currentProcess, start_TOD, curr_TOD);

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
*********************************************************************************************/
HIDDEN void getSupportData() {
    /* Step 1: The function place the support Struct in v0 */
    currentProcess->p_s.s_v0 = (int)(currentProcess->p_supportStruct); 

    /* Step 2: update the syscall CPU time for this process */
    STCK(curr_TOD); 
    updateProcessTimeHelper(currentProcess, start_TOD, curr_TOD);
    
    /* return control to the current process */
    switchContext(currentProcess); 
}

/* ---------------------------------------------------------------------------------------------- */
/* ------------------------------------- PASS UP OR DIE ----------------------------------------- */
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
void passUpOrDie(int exception_code){ 

    if (currentProcess->p_supportStruct != NULL){

        /* Step 1.1: If the current process has a support structure, pass up the exception */
        moveStateHelper(savedExceptionState, &(currentProcess->p_supportStruct->sup_exceptState[exception_code]));
        
        STCK(curr_TOD);
        updateProcessTimeHelper(currentProcess, start_TOD, curr_TOD);
        

        /* Step 1.2: perform a LDCXT using the fields from the correct sup_exceptContext field of the Current Process */
        LDCXT(
            currentProcess->p_supportStruct->sup_exceptContext[exception_code].c_stackPtr, 
            currentProcess->p_supportStruct->sup_exceptContext[exception_code].c_status,
            currentProcess->p_supportStruct->sup_exceptContext[exception_code].c_pc);
    }
    else{
        /* Call the SYS2, call the systemTrapHandler as a SYSCALL */
        sysCallNum = SYS2_NUM;
        terminateProcess(currentProcess);
        currentProcess = NULL;
        scheduler();
    }
}

/* ---------------------------------------------------------------------------------------------- */
/* ------------------------------------- TYPES OF EXCEP ----------------------------------------- */
/* ---------------------------------------------------------------------------------------------- */

/*********************************************************************************************
 * sysCallOutRangeHandler
 * 
 * @brief
 * A SYSCALL exception numbered 9 and above occurs when the Current Process
 * executes the SYSCALL instruction (Cause.ExcCode is set to 8 [Section 3.4])
 * and the contents of a0 is greater than or equal to 9.
 * The Nucleus SYSCALL exception handler should perform a standard Pass 
 * Up or Die operation using the GENERALEXCEPT index value. (Pandos page 37)
 * 
 * @protocol
 * 1. Call the passUpOrDie function with the GENERALEXCEPT index value
 * 
 * @param void
 * @return void
*********************************************************************************************/
void sysCallOutRangeHandler(){
    passUpOrDie(GENERALEXCEPT); 
}

/*********************************************************************************************
 * programTrapHandler
 * 
 * @brief
 * A Program Trap exception occurs when the Current Process attempts to perform
 * some illegal or undefined action. A Program Trap exception is defined as an
 * exception with Cause.ExcCodes of 4-7, 9-12. [Section 3.4] (Pandos page 37)
 * 
 * @protocol
 * 1. Call the passUpOrDie function with the GENERALEXCEPT index value
 * 
 * @param void
 * @return void
*********************************************************************************************/
void programTrapHandler(){
    passUpOrDie(GENERALEXCEPT);
}

/*********************************************************************************************
 * userModeTrapHandler
 * 
 * @brief
 * In particular the Nucleus should simulate a Program Trap exception when a
 * privileged service is requested in user-mode. (Pandos page 30)
 * 
 * @protocol
 * 1. Call the programTrapHandler
 * 
 * @param void
 * @return void
*********************************************************************************************/
void userModeTrapHandler() {
    programTrapHandler();
}


/*********************************************************************************************
 * tlbTrapHandler
 * 
 * @brief
 * A TLB exception occurs when µMPS3 fails in an attempt to translate a logical
 * address into its corresponding physical address. A TLB exception is defined as an
 * exception with Cause.ExcCodes of 1-3. [Section 3.4] (Pandos page 37)
 * 
 * @protocol
 * 1. Call the passUpOrDie function with the PGFAULTEXCEPT index value
 * 
 * @param void
 * @return void
*********************************************************************************************/
void tlbTrapHandler(){
    passUpOrDie(PGFAULTEXCEPT); 
}


/*********************************************************************************************
 * systemTrapHandler
 * 
 * @brief
 * This is the handler of the SYSCALL exception
 * This function has to take care of the 8 system calls
 * 
 * @note About user mode
 * IMPORTANT:
 * We need if the Current Process was executing in kernel-mode or user-mode by
 * examines the Status register in the saved exception state.
 * 
 * If a user-mode process attempts to invoke a privileged service:
 * We want the OS to treat this as an illegal operation, which is a “Reserved Instruction” (RI) Program Trap.
 * ensure that all illegal or privileged operations in user mode follow the same path in your OS
 * 
 * We will set the exception code to “Reserved Instruction” (RI) and then call the programTrapHandler.
 * Therefore, the execode 5 bit need to be set to 01010, which equals to 10 in decimal.
 * We mask the cause register by clear all the bit from 2 to 6 and mask it
 * 
 * @note
 * To avoid infinity loop of SYSCALL, the PC must be incremented by 4 prior to returning control
 * Observe that the correct processor state to load (LDST) is the saved exception state and not the obsolete 
 * processor state stored in the Current Process’s pcb.
 * 
 * 
 * @def
 * Reserved Instruction (RI): This exception is raised whenever an instruction is ill-formed, not recognizable, 
 * or is privileged and is executed in user-mode (Status.KUc=1).
 * 
 * KUc: bit 1 - The "current" kernel-mode user-mode control bit. When Status.KUc=0 the processor is in kernel-mode.
 * The status register last 5 bits: KUo IEo KUp IEp KUc IEc
 * 
 * @protocol
 * 1. Redirect if it is user mode
 *      - If the process was executing in user-mode, we need to set the exception code to “Reserved Instruction” (RI)
 *      - Invoke the programTrapHandler
 * 2. Check if the syscall number is in range
 *      - If the syscall number is not in range, we need to pass up or die thru sysCallOutRangeHandler
 * 3. Add the state to the current process
 * 3. Call the appropriate syscall function
 * 
 * @param void
 * @return void
*********************************************************************************************/
void systemTrapHandler() {

    /* Get the BIOSDATAPAGE */
    savedExceptionState = (state_PTR) BIOSDATAPAGE;
    sysCallNum = savedExceptionState->s_a0;
    savedExceptionState->s_pc += WORDLEN;
    
    /* STEP 1: redirect if it is usermode and set the exception code to RI */
    if (( (savedExceptionState->s_status) & USERPON) != ALLOFF) {

        /* Set the exception code to RI */
        savedExceptionState->s_cause &= ~(CAUSE_INT_MASK << EXC_CODE_SHIFT);
        savedExceptionState->s_cause |= (EXC_RESERVED_INSTRUCTION << EXC_CODE_SHIFT);

        /* Invoke the programTrapHandler */
        userModeTrapHandler();
    }

    /* STEP 2: check if the syscall number is in range */
    if ((sysCallNum < SYS1_NUM )|| (sysCallNum > SYS8_NUM)) {
        sysCallOutRangeHandler();
    }

    /* STEP 3: add the state to the current process */
    addPigeonCurrentProcessHelper(currentProcess);
 
    /* STEP 4: Call the appropriate syscall function */
    switch (sysCallNum) {
        case SYS1_NUM:
            createProcess(
                (state_PTR)(currentProcess->p_s.s_a1),
                (support_PTR)(currentProcess->p_s.s_a2));
            break;
        case SYS2_NUM:
            terminateProcess(currentProcess);
            currentProcess = NULL;
            scheduler();
            break;
        case SYS3_NUM:
            passeren((int *)(currentProcess->p_s.s_a1));
            break;
        case SYS4_NUM:
            verhogen((int *)(currentProcess->p_s.s_a1));
            break;
        case SYS5_NUM:
            waitForIO(currentProcess->p_s.s_a1,
                      currentProcess->p_s.s_a2,
                      currentProcess->p_s.s_a3);
            break;
        case SYS6_NUM:
            getCPUTime();
            break;
        case SYS7_NUM:
            waitForClock();
            break;
        case SYS8_NUM:
            getSupportData();
            break;
    
    }
}
