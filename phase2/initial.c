#include "../h/initial.h"
#include "../h/const.h"
#include "../h/types.h"


/* ------------------------------------------------------ */
/* ------------------ Global Variables ------------------ */
/* ------------------------------------------------------ */
unsigned int processCount;
unsigned int softBlockedCount;
pcb_PTR readyQueue;
pcb_PTR currentProcess;

semaphore semaphoreInternal; /* specific purpose for internal or "soft" blocking */
semaphore semaphoreDevices[TOTAL_IO_DEVICES];

cpu_t start_TOD;

/************************************************************************************************
 * @brief
 * The variable currentProceessorState is a pointer to the processor state saved at the time an exception occurred
 * It stores the state captured by the CPU when an event or exception is raised. 
 * Exception or interrupt handler use its value to either pass control to the appropriate exception handler or terminate the process.
 * (pandos page 24)
 * 
 * @protocol
 * The system can later restore the state if the exception is handled and the process resumes.
 * The OS can examine the state to decide if the exception can be managed or if the process should be terminated.
 * Context swtich
*********************************************************************************************/
state_PTR savedExceptionState;

static void initPassUpVector() {
    passupvector_t *pass_up_vector = (passupvector_t *) PASSUPVECTOR; // Cast the fixed memory address PASSUPVECTOR to a pointer

    pass_up_vector->tlb_refll_handler  = (memaddr) TLBRefillHandler; // Set the TLB refill handler function address in the pass-up vector
    pass_up_vector->tlb_refll_stackPtr = (memaddr) KERNELSTACK;        // Set the stack pointer (KERNELSTACK) for TLB refill handling
    pass_up_vector->execption_handler  = (memaddr) exceptionHandler;   // Set the general exception handler function address
    pass_up_vector->exception_stackPtr = (memaddr) KERNELSTACK;        // Set the stack pointer for general exceptions to KERNELSTACK
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
extern void updateProcessTimeHelper(pcb_PTR proc, cpu_t start, cpu_t end) {
    proc->p_time += (end - start);
}



extern void test();




HIDDEN void initDeviceSemaphores() {
    semaphoreInternal = 0;
    /* Set the interval timer semaphore to 0 (unblocked) 
    Initialize each device semaphore to 0 (unblocked)*/
    for (int i = 0; i < TOTAL_IO_DEVICES; i++) {
        semaphore_devices[i] = 0;
    }
}






void main() {
    initPassUpVector();

    initPcbs();
    initASL();
    
    processCount = 0;

    /* temporary blocking state where a process is waiting for a 
    resource or event but can be interrupted. 
    Unlike hard blocks, 
    soft blocks are non-critical and can be handled flexibly by the OS.
    Common examples of soft blocks include:
        Waiting for I/O operations to complete
        Waiting for timer interrupts
        Waiting for user input
        Sleep states that can be interrupted
        Key differences from hard blocks:

    Can be interrupted by signals
        Process can be woken up by the OS
        Not deadlock-prone like hard blocks
        Usually used for non-critical waiting scenarios
    */
    softBlockedCount = 0;


    readyQueue = mkEmptyProcQ();
    currentProcess = NULL;

    LDIT(PSECOND); // Load the interval timer with the value of PSECOND (100000)

    /*
    The Nucleus maintains one integer semaphore
    for each external (sub)device in µMPS3, plus one additional semaphore
    to support the Pseudo-clock.
    */
    initDeviceSemaphores();


    pcb_PTR new_process = allocPcb();

    if (new_process != NULL) {
        
        RAMTOP(new_process->p_s.s_sp);
        new_process->p_s.s_pc = (memaddr) test;
        new_process->p_s.s_t9 = (memaddr) test;
        new_process->p_s.s_status = ALLOFF | IEPON | PLTON | IMON;
        
        insertProcQ(&readyQueue, new_process);
        processCount++;
        scheduler();
        return 0;
    }

    PANIC();
    return 0;
}