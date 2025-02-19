#include "../h/nucl_initial.h"
#include "../h/const.h"
#include "../h/types.h"


/* ------------------------------------------------------ */
/* ------------------ Global Variables ------------------ */
/* ------------------------------------------------------ */




/* ----------------------------------------------------------------------- */
/* ------------------  ------------------ */
/* ----------------------------------------------------------------------- */

/**************************************************************
 * initPassUpVector -  Nucleus TLB-Refill event handler
 *
 * Initialize the pass up vector to contain the 
 * addresses of the trap handler and the new areas
 * This allows the code to update the pass-up vector with the 
 * correct handler addresses and stack pointers, 
 * ensuring that when exceptions occur, the system knows 
 * how to recover or process them appropriately.
 * 
 * @param void
 * @return void
 **************************************************************/

static void initPassUpVector() {
    passupvector_t *pass_up_vector = (passupvector_t *) PASSUPVECTOR;
    pass_up_vector->tlb_refill_handler  = (memaddr)TLBRefillHandler;
    pass_up_vector->exception_handler   = (memaddr)exceptionHandler;

}


/**************************************************************
 * resetIntervalTimer
 *
 * Reset the interval timer to 100ms
 * 
 * @param void
 * @return void
 **************************************************************/


void resetIntervalTimer() {
    /* Reset the interval timer */
    cpu_t curr_time;
    STCK(curr_time);
    LDIT(PSECOND - (curr_time % PSECOND)); /* 100ms */
}



static pcb_PTR createFirstProcess() {
    pcb_PTR new_process = allocPcb();


    return new_process;
}






void main() {
    initPassUpVector();

    initPcbs();
    initASL();
    
    process_count = 0;
    softblocked_count = 0;
    mkEmptyProcQ();
    curr_process = NULL;

    resetIntervalTimer();

    insertProcQ(&ready_queue, createFirstProcess());
    process_count++;

    scheduler();
    PANIC();
}