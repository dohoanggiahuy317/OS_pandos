/**********************************************************************************************
 * inital.case
 * 
 * @brief 
 * This file manages the initialization and operation of semaphore devices 
 * used to synchronize concurrent processes or threads. 
 * 
 * The main function initializes the pass up vector, process control blocks (PCBs),
 * active semaphore list (ASL), device semaphores, and other internal structures.
 * It sets the process count to 0, creates an empty ready queue, and sets the current process to NULL.
 * The purpose of this function is to provide a solid foundation for the scheduler and ensure system stability.
 * 
 * This file also implement the helper functions to initialize the pass up vector,
 * update the process time, and initialize the device semaphores. These functions are
 * used to set up the system and manage the execution of processes in a controlled manner.
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

#include "../h/initial.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/asl.h"
#include "../h/pcb.h"
#include "/usr/include/umps3/umps/libumps.h"


extern void test();
extern void uTLB_RefillHandler(); /* function declaration for uTLB_RefillHandler(), which will be defined in exceptions.c */

/* ------------------------------------------------------ */
/* ------------------ Global Variables ------------------ */
/* ------------------------------------------------------ */

/* Count the currnt number of process in the system */
int processCount;

/* Count the current number of soft blocked process in the system */
int softBlockedCount;

/* The ready queue is a list of PCBs that are ready to run */
pcb_PTR readyQueue;

/* The current process is the process that is currently running */
pcb_PTR currentProcess;

/* The device semaphores are used to manage access to the devices */
int semaphoreDevices[MAX_DEVICE_COUNT];

/* The start time of the current process */
cpu_t start_TOD;

/* The current time of the system */
cpu_t curr_TOD;

/* The interrupt time of the system */
cpu_t interrupt_TOD;

/* the exception processor state to handle */
state_PTR savedExceptionState;





/* ---------------------------------------------------------------------------------------------- */
/* ------------------------------------- HELPER + HANDLER --------------------------------------- */
/* ---------------------------------------------------------------------------------------------- */

/*********************************************************************************************
 * debugExceptionHandler
 * 
 * @brief
 * Debug function for exception handling. Accepts four integer parameters that can be viewed
 * in registers a0-a3 when a breakpoint is set on this function.
 * 
 * @param key - Unique identifier showing where in the code this debug call happens
 * @param param1 - First parameter (typically exception code)
 * @param param2 - Second parameter (typically cause register value)
 * @param param3 - Third parameter (typically EPC or other state info)
 * 
 * @return void
***********************************************************************************************/
void debugExceptionHandler(int key, int param1, int param2, int param3) {}

/************************************************************************************************
 * initPassUpVector
 * 
 * @brief
 * This function initializes the pass up vector, which is a structure in memory that helps the BIOS
 * know where to transfer control when an exception occurs. The pass up vector is used to pass
 * exceptions up to the OS for handling. The function sets the TLB Refill handler, TLB Refill stack
 * pointer, exception handler, and exception stack pointer in the pass up vector.
 * 
 * @protocol
 * 0. Create a pointer to the pass up vector
 * 1. Set the TLB Refill handler to the uTLB_RefillHandler function
 * 2. Set the TLB Refill stack pointer to the KERNELSTACK
 * 3. Set the exception handler to the exceptionHandler function
 * 4. Set the exception stack pointer to the KERNELSTACK
 * 
 * @note
 * The system can later restore the state if the exception is handled and the process resumes.
 * The OS can examine the state to decide if the exception can be managed or if the process should be terminated.
 * 
 * @note
 * The Pass Up Vector is a structure in memory that helps the BIOS 
 * know where to transfer control when an exception occurs.
 * It kinda like bring/taking the exception to the OS.
 * 
 * @param void
 * @return void
*********************************************************************************************/
void initPassUpVector() {
    /* STEP 0: create a pointer */
    passupvector_t *pass_up_vector = (passupvector_t *) PASSUPVECTOR;

    /* STEP 1 + 2 + 3 + 4: set the TLB Refill handler, TLB Refill stack pointer, 
    exception handler, and exception stack pointer */
    pass_up_vector->tlb_refll_handler  = (memaddr) uTLB_RefillHandler;
    pass_up_vector->tlb_refll_stackPtr = (memaddr) KERNELSTACK;    
    pass_up_vector->exception_handler  = (memaddr) exceptionHandler; 
    pass_up_vector->exception_stackPtr = (memaddr) KERNELSTACK; 
}

/*********************************************************************************************
 * updateProcessTimeHelper
 * 
 * @brief
 * This is a helper function, which is used to update the p_time time of a process.
 * The function takes the process, start time, and end time as arguments.
 * It updates the process time by adding the difference between the end time and start time.
 * 
 * @protocol
 * 1. Update the process time by adding the difference between the end time and start time
 * 
 * @def
 * p_time: The accumulated processor time used by the process.
 * 
 * @note
 * In phase 2, most of the time this function will be call to update the process time
 * of the current process.
 * 
 * @param pcb_PTR proc: pointer to the process
 * @param cpu_t start: start time
 * @param cpu_t end: end time
 * @return void
 ***********************************************************************************************/
void updateProcessTimeHelper(pcb_PTR process, cpu_t start, cpu_t end) {
    process->p_time += (end - start);
}

/*********************************************************************************************
 * initDeviceSemaphoresHelper
 * 
 * @brief
 * This function is used to initialize the device semaphores.
 * Set all the device semaphores to 0 (unblocked).
 * 
 * @protocol
 * 1. Initialize each device semaphore to 0 (unblocked)
 * 
 * @note 
 * The total number of the device semaphores is equal to the MAX_DEVICE_COUNT, which is 49, including:
 *      - for each external (sub)device in µMPS3, plus one additional semaphore to support the Pseudo-clock.
 * The device semaphre will be used gfor synch. (Pandos page 20)
 * 
 * @param void
 * @return void
 ***********************************************************************************************/
void initDeviceSemaphoresHelper() {
    /* Set the interval timer semaphore to 0 (unblocked) 
    Initialize each device semaphore to 0 (unblocked)*/
    int i;
    for (i = 0; i < MAX_DEVICE_COUNT; i++) {
        semaphoreDevices[i] = 0;
    }
}


/* ---------------------------------------------------------------------------------------------- */
/* ------------------------------------------ MAIN :) ------------------------------------------- */
/* ---------------------------------------------------------------------------------------------- */


/*********************************************************************************************
 * main
 * 
 * @brief
 * This function serves as the critical entry point of the OS kernel. Its role is to set up 
 * essential subsystems that enable the OS to manage processes, handle exceptions, and ensure 
 * proper timing of operations. 
 * By initializing the pass up vector, PCBs, the active semaphore list, 
 * device semaphores, and other internal structures, this function provides the necessary foundation 
 * for the scheduler and overall system stability. It is a necessary and integral part of the OS because 
 * without proper initialization, there would be no reliable mechanism to manage system resources or 
 * to react to events and exceptions.
 * 
 * @protocol
 * (Pandos page 19-22)
 * 1. Initialize the pass up vector
 * 2. Initialize the process control blocks (PCBs)
 * 3. Initialize the active semaphore list (ASL)
 * 4. Initialize the process count to 0
 * 5. Initialize the soft blocked count to 0
 * 6. Create an empty ready queue
 * 7. Set the current process to NULL
 * 8. Initialize the device semaphores
 * 9. Load the interval timer with the value of PSECOND (100000)
 * 10. Allocate a new process and set its initial state
 *      - Set the stack pointer to the top of the RAM
 *      - Set the program counter to the test function
 *      - Enable interrupts
 *      - Insert the new process into the ready queue
 *      - Increment the process count
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
 * @note
 * when init the single process, we need to allocate a new process and set its initial state.
 * IT has interrupts enabled, the stack pointer is set to the top of the RAM, and the program counter
 * is set to the test function. (Pandos page21)
 * 
 * The RAMTOP is the top of the RAM, and the RAMBASE is the base of the RAM. Hence, the stack pointer can
 * be set to the top of the RAM by adding the RAMBASE to the RAMTOP.
 * 
 * @param void
 * @return int
 ***********************************************************************************************/
void main() {

    /* Step 1: init the pass up vector */
    initPassUpVector();

    /* step 1+3: init PCB and ACL */
    initPcbs();
    initASL();


    /* Step 4 5 6 7: init the process count */
    processCount = 0;
    softBlockedCount = 0;
    readyQueue = mkEmptyProcQ();
    currentProcess = NULL;
    
    /* Step 8: init the device semaphores */
    initDeviceSemaphoresHelper();

    /* Step 9: load the interval timer with the value of PSECOND (100000)
    this can be use for scheduling */
    LDIT(PSECOND); 

    /* Step 10: allocate a new process and set its initial state */
    pcb_PTR new_process = allocPcb();
   

    if (new_process != NULL) { /* if allocate is succesful */
        
        /* set the initial state of the new process */
        devregarea_t *device_register_area;
        device_register_area = (devregarea_t *) RAMBASEADDR;

        /* set the initial state of the new process */
        new_process->p_s.s_sp = device_register_area->rambase + device_register_area->ramsize;
        new_process->p_s.s_pc = (memaddr) test;
        new_process->p_s.s_t9 = (memaddr) test;
        new_process->p_s.s_status = ALLOFF | IEPON | PLTON | IMON;
        
        /* insert the new process into the ready queue */
        insertProcQ(&readyQueue, new_process);
        processCount++;
        scheduler();
    }

    /* If the allocation fails, the system is in a panic state */
    PANIC();
}
