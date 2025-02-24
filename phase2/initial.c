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





static void initPassUpVector() {
    passupvector_t *pass_up_vector = (passupvector_t *) PASSUPVECTOR; // Cast the fixed memory address PASSUPVECTOR to a pointer

    pass_up_vector->tlb_refll_handler  = (memaddr) TLBRefillHandler; // Set the TLB refill handler function address in the pass-up vector
    pass_up_vector->tlb_refll_stackPtr = (memaddr) KERNELSTACK;        // Set the stack pointer (KERNELSTACK) for TLB refill handling
    pass_up_vector->execption_handler  = (memaddr) exceptionHandler;   // Set the general exception handler function address
    pass_up_vector->exception_stackPtr = (memaddr) KERNELSTACK;        // Set the stack pointer for general exceptions to KERNELSTACK
}


extern void test();

static void initDeviceSemaphores() {
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