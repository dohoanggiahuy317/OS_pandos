#ifndef TYPES
#define TYPES

/****************************************************************************
 *
 * This header file contains utility types definitions.
 *
 ****************************************************************************/

#include "../h/const.h"

typedef signed int cpu_t;
typedef unsigned int memaddr;

/* Device Register */
typedef struct {
    unsigned int d_status;
    unsigned int d_command;
    unsigned int d_data0;
    unsigned int d_data1;
} device_t;

#define t_recv_status d_status
#define t_recv_command d_command
#define t_transm_status d_data0
#define t_transm_command d_data1

/* Bus Register Area */
typedef struct {
    unsigned int rambase;
    unsigned int ramsize;
    unsigned int execbase;
    unsigned int execsize;
    unsigned int bootbase;
    unsigned int bootsize;
    unsigned int todhi;
    unsigned int todlo;
    unsigned int intervaltimer;
    unsigned int timescale;
    unsigned int TLB_Floor_Addr;
    unsigned int inst_dev[DEVINTNUM];
    unsigned int interrupt_dev[DEVINTNUM];
    device_t devreg[DEVINTNUM * DEVPERINT];
} devregarea_t;

/* Pass Up Vector */
typedef struct passupvector {
    unsigned int tlb_refll_handler;
    unsigned int tlb_refll_stackPtr;
    unsigned int exception_handler;
    unsigned int exception_stackPtr;
} passupvector_t;

#define STATEREGNUM 31
typedef struct state_t {
    unsigned int s_entryHI;
    unsigned int s_cause;
    unsigned int s_status;
    unsigned int s_pc;
    int s_reg[STATEREGNUM];

} state_t, *state_PTR;

#define s_at s_reg[0]
#define s_v0 s_reg[1]
#define s_v1 s_reg[2]
#define s_a0 s_reg[3]
#define s_a1 s_reg[4]
#define s_a2 s_reg[5]
#define s_a3 s_reg[6]
#define s_t0 s_reg[7]
#define s_t1 s_reg[8]
#define s_t2 s_reg[9]
#define s_t3 s_reg[10]
#define s_t4 s_reg[11]
#define s_t5 s_reg[12]
#define s_t6 s_reg[13]
#define s_t7 s_reg[14]
#define s_s0 s_reg[15]
#define s_s1 s_reg[16]
#define s_s2 s_reg[17]
#define s_s3 s_reg[18]
#define s_s4 s_reg[19]
#define s_s5 s_reg[20]
#define s_s6 s_reg[21]
#define s_s7 s_reg[22]
#define s_t8 s_reg[23]
#define s_t9 s_reg[24]
#define s_gp s_reg[25]
#define s_sp s_reg[26]
#define s_fp s_reg[27]
#define s_ra s_reg[28]
#define s_HI s_reg[29]
#define s_LO s_reg[30]

/*********************************************************************************************
 * @brief Context Descriptor
 * 
 * This data structure is used to represent a context descriptor
*********************************************************************************************/

typedef struct context_t {
    /* process context fields */
    unsigned int    c_stackPtr, /* stack pointer value */
                    c_status, /* status reg value */
                    c_pc; /* PC address */
} context_t, *context_PTR;



/*********************************************************************************************
 * @brief Support Descriptor
 * 
 * This data structure is used to represent a support descriptor
*********************************************************************************************/

typedef struct support_t {
    int         sup_asid; /* Process Id (asid) */
    state_t     sup_exceptState[2]; /* stored excpt states */
    context_t   sup_exceptContext[2]; /* pass up contexts */
} support_t, *support_PTR;

/*********************************************************************************************
 * @brief Process Control Block
 * 
 * This data structure is used to represent a process control block
*********************************************************************************************/

typedef struct pcb_t {
    /* process queue fields */
    struct pcb_t    *p_next,  /* pointer to next entry */
                    *p_prev,  /* pointer to prev entry */

                    /* process tree fields */            
                    *p_prnt,  /* pointer to parent */
                    *p_child, /* pointer to 1st child */
                    *p_lSib,  /* pointer to sibling */
                    *p_rSib;  /* pointer to sibling */
                
    /* process status information */
    state_t p_s;   /* processor state */
    cpu_t p_time;  /* cpu time used by proc */

    /* 
    * This pointer tells you on which semaphore (from the ASL) the process is waiting. 
    If the process is not blocked, p_semAdd is NULL.
    */
    int *p_semAdd; /* pointer to sema4 on which process blocked */
    
    /* support layer information */
    support_t *p_supportStruct; /* ptr to support struct */
    

} pcb_t, *pcb_PTR;




/*********************************************************************************************
 * @brief Semaphore Descriptor
 * 
 * This data structure is used to represent a semaphore descriptor
*********************************************************************************************/

typedef struct semd_t {
    struct semd_t *s_next; /* next element on the ASL */
    int *s_semAdd; /* pointer to the semaphore*/
    pcb_t *s_procQ; /* tail pointer to a process queue */
} semd_t, *semd_PTR;



/**
 * @brief Semaphore type
 */
typedef int semaphore;

#endif
