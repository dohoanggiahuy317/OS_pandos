#include "../h/asl.h"
#include "../h/pcb.h"
#include "../h/const.h"
#include <stddef.h>

/**************************************************************
 * The maximum number of semaphore
 * We reserve two dummy nodes and one descriptor for each process in the system
**************************************************************/
#define MAXSEMDS (MAXPROC + 2)


/* 
 * Private (static) global variables for the ASL module.
 *
 * semd_h points to the dummy head of the ASL.
 * semdFree_h points to the free list of semaphore descriptors.
 * semdTable is a static array of semaphore descriptors.
 */
static semd_t *semd_h;         /* Head pointer for the active semaphore list (points to dummy head) */
static semd_t *semdFree_h;     /* Head pointer for the free list */
static semd_t semdTable[MAXSEMDS];  /* Array of semaphore descriptors */

/* 
 * A private helper function that traverses the ASL (which always begins with
 * a dummy head) and returns a pointer to the first semaphore descriptor whose key
 * is not less than semAdd. It also returns (via the out-parameter *prev) the pointer
 * to the node immediately preceding the returned node.
 *
 * Since s_semAdd is a pointer, we cast it to unsigned long for comparison.
 */
static semd_t *getSemd (int *semAdd, semd_t **prev) {
    semd_t *curr = semd_h;  /* start at dummy head */
    semd_t *parent = NULL;
    
    /* Traverse while the key of the current node is less than semAdd */
    while (curr != NULL && ((unsigned long)curr->s_semAdd < (unsigned long) semAdd)) {
        parent = curr;
        curr = curr->s_next;
    }
    
    if (prev)
        *prev = parent;
    return curr;
}

/*
 * int insertBlocked(int *semAdd, pcb_t *p)
 *
 * Insert the process control block pointed to by p at the tail of the process
 * queue associated with the semaphore whose physical address is semAdd.
 *
 * If a semaphore descriptor for semAdd is not already present in the ASL, then
 * allocate a new descriptor from the semdFree list, initialize it (setting its
 * s_semAdd field to semAdd and its s_procQ field to an empty process queue), and
 * insert it in sorted order into the ASL.
 *
 * In either case, p is then inserted at the tail of the process queue for semAdd,
 * and p->p_semAdd is set to semAdd.
 *
 * If a new semaphore descriptor is needed but the semdFree list is empty, return TRUE.
 * Otherwise, return FALSE.
 */
int insertBlocked (int *semAdd, pcb_t *p) {
    semd_t *prev, *curr;
    
    /* Search the ASL for a descriptor with key equal to semAdd */
    curr = getSemd(semAdd, &prev);
    
    /* If not found (i.e. the found node’s key is not equal to semAdd), allocate a new descriptor */
    if (curr == NULL || ((unsigned long) curr->s_semAdd != (unsigned long) semAdd)) {
        /* If semdFree list is empty, we cannot allocate a new descriptor. */
        if (semdFree_h == NULL)
            return 1;  /* TRUE: error condition */
        
        /* Remove a descriptor from the free list */
        semd_t *newSemd = semdFree_h;
        semdFree_h = semdFree_h->s_next;
        
        /* Initialize the new semaphore descriptor */
        newSemd->s_semAdd = semAdd;
        newSemd->s_procQ = mkEmptyProcQ();  /* mkEmptyProcQ() returns NULL for an empty process queue */
        
        /* Insert newSemd into the ASL between prev and curr */
        newSemd->s_next = curr;
        if (prev != NULL)
            prev->s_next = newSemd;
        else
            semd_h = newSemd;  /* Should not occur since we always have a dummy head */
        
        curr = newSemd;
    }
    
    /* Set the semaphore address in the PCB */
    p->p_semAdd = semAdd;
    /* Insert p at the tail of the process queue associated with this semaphore descriptor */
    insertProcQ(&(curr->s_procQ), p);
    
    return 0;  /* FALSE: no error */
}

/*
 * pcb_t *removeBlocked(int *semAdd)
 *
 * Searches the ASL for a descriptor for semaphore semAdd.
 * If not found, returns NULL.
 * Otherwise, removes the first (i.e. head) PCB from that semaphore's process queue,
 * sets that PCB's p_semAdd to NULL, and returns the PCB pointer.
 *
 * If the process queue becomes empty as a result, the semaphore descriptor is removed
 * from the ASL and returned to the semdFree list.
 */
pcb_t *removeBlocked (int *semAdd) {
    semd_t *prev, *curr;
    
    curr = getSemd(semAdd, &prev);
    /* Check that we found a descriptor with key equal to semAdd */
    if (curr == NULL || ((unsigned long) curr->s_semAdd != (unsigned long) semAdd))
        return NULL;
    
    /* Remove the head of the process queue */
    pcb_t *removed = removeProcQ(&(curr->s_procQ));
    if (removed != NULL)
        removed->p_semAdd = NULL;
    
    /* If the process queue is now empty, remove the semaphore descriptor from the ASL */
    if (emptyProcQ(curr->s_procQ)) {
        if (prev != NULL)
            prev->s_next = curr->s_next;
        /* Return the descriptor to the free list */
        curr->s_next = semdFree_h;
        semdFree_h = curr;
    }
    
    return removed;
}

/*
 * pcb_t *outBlocked(pcb_t *p)
 *
 * Removes the PCB pointed to by p from the process queue associated with p->p_semAdd.
 * If p is not found on that queue, returns NULL. (This is an error condition.)
 *
 * Unlike removeBlocked, p->p_semAdd is NOT reset to NULL.
 *
 * If after removal the process queue becomes empty, the corresponding semaphore
 * descriptor is removed from the ASL and returned to the semdFree list.
 */
pcb_t *outBlocked (pcb_t *p) {
    semd_t *prev, *curr;
    
    if (p == NULL || p->p_semAdd == NULL)
        return NULL;
    
    /* Locate the semaphore descriptor for p->p_semAdd */
    curr = getSemd(p->p_semAdd, &prev);
    if (curr == NULL || ((unsigned long) curr->s_semAdd != (unsigned long) p->p_semAdd))
        return NULL;
    
    /* Remove p from the process queue */
    pcb_t *removed = outProcQ(&(curr->s_procQ), p);
    if (removed == NULL)
        return NULL;  /* p was not found in the process queue */
    
    /* If the process queue is now empty, remove the descriptor from the ASL */
    if (emptyProcQ(curr->s_procQ)) {
        if (prev != NULL)
            prev->s_next = curr->s_next;
        curr->s_next = semdFree_h;
        semdFree_h = curr;
    }
    
    return removed;
}

/*
 * pcb_t *headBlocked(int *semAdd)
 *
 * Returns the PCB at the head of the process queue associated with semaphore semAdd.
 * Returns NULL if semAdd is not found in the ASL or if its process queue is empty.
 */
pcb_t *headBlocked (int *semAdd) {
    semd_t *prev, *curr;
    
    curr = getSemd(semAdd, &prev);
    if (curr == NULL || ((unsigned long) curr->s_semAdd != (unsigned long) semAdd))
        return NULL;
    
    return headProcQ(curr->s_procQ);
}

/*
 * void initASL(void)
 *
 * Initializes the Active Semaphore List.
 *
 * A static array of semaphore descriptors (semdTable) is declared.
 * Two descriptors are reserved for dummy nodes: the dummy head (with key 0)
 * and dummy tail (with key MAXINT). The dummy head’s s_next points to the dummy tail.
 * The remaining descriptors (indices 2 ... MAXSEMDS-1) are placed on the semdFree list.
 */
void initASL () {
    int i;
    semd_t *dummyTail;
    
    /* Initialize dummy head (index 0) */
    semd_h = &semdTable[0];
    semd_h->s_semAdd = (int *) 0;   /* dummy key: 0 */
    semd_h->s_procQ  = mkEmptyProcQ(); /* an empty process queue (NULL) */
    
    /* Initialize dummy tail (index 1) */
    dummyTail = &semdTable[1];
    dummyTail->s_semAdd = (int *) 1000;  /* dummy key: MAXINT (assumed defined in const.h) */
    dummyTail->s_procQ  = mkEmptyProcQ();
    
    /* Link the dummy head to dummy tail */
    semd_h->s_next = dummyTail;
    dummyTail->s_next = NULL;
    
    /* Initialize the free list with the remaining semaphore descriptors.
       They will be used as needed when a new semaphore descriptor is required.
       We add them to the free list in descending order of index.
    */
    semdFree_h = NULL;
    for (i = MAXSEMDS - 1; i >= 2; i--) {
        semd_t *s = &semdTable[i];
        s->s_semAdd = NULL;
        s->s_procQ  = mkEmptyProcQ();
        s->s_next   = semdFree_h;
        semdFree_h  = s;
    }
}