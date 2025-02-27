/**********************************************************************************************
 * inital.case
 * 
 * @brief 
 * This file manages the initialization and operation of semaphore devices 
 * used to synchronize concurrent processes or threads. It is important because
 * it provides a reliable mechanism to control access to shared resources, ensuring
 * that operations are performed in a thread-safe manner. The file encapsulates the 
 * core concept of preventing race conditions by coordinating access through semaphores.
 *
 * @def 
 * - semaphore: A global semaphore variable used to gate access to critical portions 
 * of the code. It ensures that only one thread or process is allowed to 
 * enter a protected section at any given time.
 * 
 * - device_initialized: A flag to indicate whether the semaphore devices have 
 * been properly created and initialized. It is crucial for 
 * preventing redundant initialization and ensuring proper operation.
 *
 * @note  
 * The creation of semaphore devices is an essential operation. Semaphores are needed
 * to ensure effective synchronization across different threads/processes. They are 
 * created using system or library calls to allocate and initialize resources which 
 * then manage the access to shared resources. This design helps avoid race conditions 
 * and maintains system stability during concurrent operations.
 * 
 * @author
 * JaWeee Do
**********************************************************************************************/


#include "../h/types.h"
#include "../h/const.h"
#include "../h/initial.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../h/scheduler.h"
#include "../h/pcb.h"
#include "../h/asl.h"
#include "/usr/include/umps3/umps/libumps.h"


extern void test();

/* ------------------------------------------------------ */
/* ------------------ Global Variables ------------------ */
/* ------------------------------------------------------ */

/* Count the currnt number of process in the system */
unsigned int processCount;

/* Count the current number of soft blocked process in the system */
unsigned int softBlockedCount;

/* The ready queue is a list of PCBs that are ready to run */
pcb_PTR readyQueue;

/* The current process is the process that is currently running */
pcb_PTR currentProcess;

/* The device semaphores are used to manage access to the devices */
semaphore semaphoreInternal;

/* The device semaphores are used to manage access to the devices */
semaphore semaphoreDevices[MAX_DEVICE_COUNT];

/* The start time of the current process */
cpu_t start_TOD;

/* the exception processor state to handle */
state_PTR savedExceptionState;


/* ---------------------------------------------------------------------------------------------- */
/* ------------------------------------------ MAIN :) ------------------------------------------- */
/* ---------------------------------------------------------------------------------------------- */


/*********************************************************************************************
 * main
 * 
 * @brief
 * This function serves as the critical entry point of the OS kernel. Its role is to set up 
 * essential subsystems that enable the OS to manage processes, handle exceptions, and ensure 
 * proper timing of operations. By initializing the pass up vector, PCBs, the active semaphore list, 
 * device semaphores, and other internal structures, this function provides the necessary foundation 
 * for the scheduler and overall system stability. It is a necessary and integral part of the OS because 
 * without proper initialization, there would be no reliable mechanism to manage system resources or 
 * to react to events and exceptions.
 * 
 * @protocol
 * 1. Initialize the pass up vector
 * 2. Initialize the process control blocks (PCBs)
 * 3. Initialize the active semaphore list (ASL)
 * 4. Initialize the process count to 0
 * 5. Initialize the soft blocked count to 0
 * 6. Create an empty ready queue
 * 7. Set the current process to NULL
 * 8. Load the interval timer with the value of PSECOND (100000)
 * 9. Initialize the device semaphores
 * 10. Allocate a new process and set its initial state
 * 
 * @def
 * - Pass Up Vector is part of the BIOS Data Page, and for Processor 0, 
 * is located at 0x0FFF.F900.
 * 
 * - Soft blocking state where a process is waiting for a resource or event but can be interrupted. 
 * Common examples of soft blocks include:
 *      Waiting for I/O operations to complete
 *      Waiting for timer interrupts
 *      Waiting for user input
 *      Sleep states that can be interrupted
 * 
 * @param void
 * @return int
 ***********************************************************************************************/

void main() {

    /* Step 1: init the pass up vector */
    initPassUpVector();

    /* step 2 + 3: init PCB and ACL */
    initPcbs();
    initASL();
    
    /* Step 4: init the process count */
    processCount = 0;

    /* Step 5: init the soft blocked count */
    softBlockedCount = 0;

    /* Step 6: create an empty ready queue */
    readyQueue = mkEmptyProcQ();

    /* Step 7: set the current process to NULL */
    currentProcess = NULL;

    /* Step 8: load the interval timer with the value of PSECOND (100000)
    this can be use for scheduling */
    LDIT(PSECOND); 

    /* Step 9: init the device semaphores */
    initDeviceSemaphores();

    /* Step 10: allocate a new process and set its initial state */
    pcb_PTR new_process = allocPcb();

    if (new_process != NULL) { /* if allocate is succesful */
        
        /* set the initial state of the new process */
        RAMTOP(new_process->p_s.s_sp);
        new_process->p_s.s_pc = (memaddr) test;
        new_process->p_s.s_t9 = (memaddr) test;
        new_process->p_s.s_status = ALLOFF | IEPON | PLTON | IMON;
        
        /* insert the new process into the ready queue */
        insertProcQ(&readyQueue, new_process);
        processCount++;
        scheduler();
        return 0;
    }

    /* If the allocation fails, the system is in a panic state */
    PANIC();
    return 0;
}

/* ---------------------------------------------------------------------------------------------- */
/* ------------------------------------- HELPER + HANDLER --------------------------------------- */
/* ---------------------------------------------------------------------------------------------- */


/************************************************************************************************
 * initPassUpVector
 * 
 * @brief
 * The variable currentProceessorState is a pointer to the processor state saved at the time an exception occurred
 * It stores the state captured by the CPU when an event or exception is raised. 
 * Exception or interrupt handler use its value to either pass control to the appropriate exception handler or terminate the process.
 * (pandos page 24)
 * 
 * @protocol
 * 0. Create a pointer to the pass up vector
 * 1. Set the TLB Refill handler to the uTLB_RefillHandler function
 * 2. Set the TLB Refill stack pointer to the KERNELSTACK
 * 3. Set the exception handler to the exceptionTrapHandler function
 * 4. Set the exception stack pointer to the KERNELSTACK
 * 
 * @note
 * The system can later restore the state if the exception is handled and the process resumes.
 * The OS can examine the state to decide if the exception can be managed or if the process should be terminated.
 * 
 * @note
 * The Pass Up Vector is a structure in memory that helps the BIOS 
 * know where to transfer control when an exception occurs.
 * It is similar to a table of contents for exception handling (or a transformer :)?)
 * 
 * @param void
 * @return void
*********************************************************************************************/

static void initPassUpVector() {
    /* STEP 0: create a pointer */
    passupvector_t *pass_up_vector = (passupvector_t *) PASSUPVECTOR;

    /* STEP 1 + 2 + 3 + 4: set the TLB Refill handler, TLB Refill stack pointer, 
    exception handler, and exception stack pointer */
    pass_up_vector->tlb_refll_handler  = (memaddr) uTLB_RefillHandler;
    pass_up_vector->tlb_refll_stackPtr = (memaddr) KERNELSTACK;    
    pass_up_vector->execption_handler  = (memaddr) exceptionTrapHandler; 
    pass_up_vector->exception_stackPtr = (memaddr) KERNELSTACK; 
}

/*********************************************************************************************
 * updateProcessTimeHelper
 * 
 * @brief
 * This function is used to update the process time of the current process.
 * The function takes the process, start time, and end time as arguments.
 * It updates the process time by adding the difference between the end time and start time.
 * 
 * @protocol
 * 1. Update the process time by adding the difference between the end time and start time
 * 
 * @param pcb_PTR proc: pointer to the process
 * @param cpu_t start: start time
 * @param cpu_t end: end time
 * @return void
 ***********************************************************************************************/

void updateProcessTimeHelper(pcb_PTR proc, cpu_t start, cpu_t end) {
    proc->p_time += (end - start);
}

/*********************************************************************************************
 * initDeviceSemaphoresHelper
 * 
 * @brief
 * This function is used to initialize the device semaphores.
 * The function sets the interval timer semaphore to 0 (unblocked) 
 * and initializes each device semaphore to 0 (unblocked).
 * 
 * @protocol
 * 1. Set the interval timer semaphore to 0 (unblocked) 
 * 2. Initialize each device semaphore to 0 (unblocked)
 * 
 * @note 
 * The Nucleus maintains one integer semaphorr
 * for each external (sub)device in µMPS3, plus one additional semaphore
 * to support the Pseudo-clock.
 * 
 * @param void
 * @return void
 ***********************************************************************************************/

HIDDEN void initDeviceSemaphoresHelper() {
    semaphoreInternal = 0;
    /* Set the interval timer semaphore to 0 (unblocked) 
    Initialize each device semaphore to 0 (unblocked)*/
    for (int i = 0; i < MAX_DEVICE_COUNT; i++) {
        semaphoreDevices[i] = 0;
    }
}