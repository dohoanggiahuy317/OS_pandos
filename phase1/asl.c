#include "../h/asl.h"
#include "../h/pcb.h"
#include "../h/const.h"
#include <stddef.h>


/* ------------------------------------------------------ */
/* ------------------ Global Variables ------------------ */
/* ------------------------------------------------------ */

/**************************************************************
 * The maximum number of semaphore
 * We reserve two dummy nodes and one descriptor for each process in the system
**************************************************************/

#define MAXSEMDS (MAXPROC + 2)

/**************************************************************
 * The semaphore descriptor
**************************************************************/

static semd_t *semd_h;
static semd_t *semdFree_h;
static semd_t semdTable[MAXSEMDS];

/* ---------------------------------------------------------------------- */
/* --------------------- The Active Semaphore List -----------------------*/
/* ---------------------------------------------------------------------- */

/**************************************************************
 * freeSemd()
 * 
 * Frees the semaphore descriptor pointed to by s by adding it to the free list
 * 
 * @param void
 * @return void
**************************************************************/

void freeSemd (semd_t *s) {
    if (s == NULL) {
        return;
    }

    /* If free list is empty */
    if (semdFree_h == NULL) {
        s->s_next = NULL;
        semdFree_h = s;
        return;
    }

    /* Add s to the head of the free list */
    s->s_next = semdFree_h;
    semdFree_h = s;
    return;
}



/**************************************************************
 * allocSemd()
 * 
 * Allocates a semaphore descriptor from the free list
 * 
 * @param void
 * @return void
**************************************************************/

semd_PTR allocSemd () {
    semd_t *newSemd;
    
    /* If free list is empty */
    if (semdFree_h == NULL) {
        return NULL;
    }
    
    /* Remove a descriptor from the free list */
    newSemd = semdFree_h;
    semdFree_h = semdFree_h->s_next;
    newSemd->s_next = NULL;
    
    return newSemd;
}

/**************************************************************
 * void initASL()
 *
 * Initializes the Active Semaphore List
 * Two descriptors are reserved for dummy nodes: 
 * the dummy head (=0)
 * and dummy tail (=MAXINT)
 * 
 * @param void
 * @return void
**************************************************************/

void initASL () {
    int i;
    semd_t *dummyTail;
    
    /* Initialize the free list with the remaining semaphore descriptors */
    semdFree_h = NULL;
    for (i = 0; i <= MAXSEMDS; i++) {
        freeSemd(&semdTable[i]);
    }

    /* Initialize dummy head (index 0) */
    semd_h = allocSemd();
    semd_h->s_semAdd = (int *) 0;  /* dummy key: 0 */
    semd_h->s_procQ = mkEmptyProcQ();
    
    /* Initialize dummy tail (index 1) */
    dummyTail = allocSemd();
    dummyTail->s_semAdd = (int *) MAXINT;  /* dummy key: MAXINT */
    dummyTail->s_procQ = mkEmptyProcQ();
    
    /* Link the dummy head to dummy tail */
    semd_h->s_next = dummyTail;
    dummyTail->s_next = NULL;
    return;
}




/**************************************************************
 * semd_t *getSemd(int *semAdd, semd_t **prev)
 * 
 * A private helper function that traverses the ASL (which always begins with
 * a dummy head) and returns a pointer to the first semaphore descriptor whose key
 * is not less than semAdd. It also returns (via the out-parameter *prev) the pointer
 * to the node immediately preceding the returned node.
 * 
 * Since s_semAdd is a pointer, we cast it to unsigned long for comparison.
 * 
 * @param semAdd: the semaphore address
 * @param prev: the pointer to the previous node
 * @return semd_t *: the semaphore descriptor
 **************************************************************/

static semd_t *getSemd (int *semAdd, semd_PTR *prev) {
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


/* ---------------------------------------------------------------------- */
/* ----------------------------- METHODS -------------------------------- */
/* ---------------------------------------------------------------------- */


/**************************************************************
 * int insertBlocked(int *semAdd, pcb_t *p)
 *
 * Insert the process control block with the semaphore whose physical address is semAdd.
 *
 * If a semaphore descriptor for semAdd is not already present in the ASL, then
 * allocate a new descriptor from the semdFree list, initialize it, and
 * insert it in sorted order into the ASL.
 *
 * If a new semaphore descriptor is needed but the semdFree list is empty, return TRUE.
 * Otherwise, return FALSE.
 * 
 * @param semAdd: the semaphore address
 * @param p: the process control block
 * @return int: TRUE if a new descriptor is needed but the semdFree list is empty, 
 * FALSE otherwise
**************************************************************/


int insertBlocked (int *semAdd, pcb_PTR p) {
    semd_t *prev, *curr;
    
    /* Search the ASL for a descriptor with key equal to semAdd */
    curr = getSemd(semAdd, &prev);
    
    /* If not found -> allocate a new descriptor */
    if (curr == NULL || ((unsigned long) curr->s_semAdd != (unsigned long) semAdd)) {
        /* If semdFree list is empty, we cannot allocate a new descriptor. */
        if (semdFree_h == NULL) {
            return TRUE;
        }
        
        /* Remove a descriptor from the free list */
        semd_t *newSemd = allocSemd();
        
        /* Initialize the new semaphore descriptor */
        newSemd->s_semAdd = semAdd;
        newSemd->s_procQ = mkEmptyProcQ();
        
        /* Insert newSemd into the ASL between prev and curr */
        newSemd->s_next = curr;
        prev->s_next = newSemd;
        curr = newSemd;
    }
    
    p->p_semAdd = semAdd;    /* Set the semaphore address in the PCB */
    insertProcQ(&(curr->s_procQ), p);     /* Insert p at the tail of the process queue with this semaphore */

    
    return FALSE;
}

/**************************************************************
 * removeBlocked
 *
 * Searches the ASL for a descriptor for semaphore semAdd.
 * removes the first PCB from that semaphore's process queue,
 * sets that PCB's p_semAdd to NULL, and returns the PCB pointer.
 *
 * If the process queue becomes empty as a result, the semaphore descriptor is removed
 * from the ASL and returned to the semdFree list.
 * 
 * @param semAdd: the semaphore address
 * @return pcb_t *: the removed PCB, or NULL if the semaphore descriptor was not found
**************************************************************/

pcb_PTR removeBlocked (int *semAdd) {

    /* Find the semaphore descriptor for semAdd */
    semd_t *prev, *curr;
    curr = getSemd(semAdd, &prev);

    /* Check that we found a descriptor with key equal to semAdd */
    if (curr == NULL || ((unsigned long) curr->s_semAdd != (unsigned long) semAdd)) {
        return NULL;
    }
    
    /* Remove the head of the process queue */
    pcb_t *removed = removeProcQ(&(curr->s_procQ));
    if (removed != NULL) {
        removed->p_semAdd = NULL;
    }
    
    /* If the process queue is now empty, remove the semaphore descriptor from the ASL */
    if (emptyProcQ(curr->s_procQ)) {
        if (prev != NULL) {
            prev->s_next = curr->s_next;
        }
        freeSemd(curr); /* Return the descriptor to the free list */
    }
    
    return removed;
}

/**************************************************************
 * outBlocked
 *
 * Removes the PCB from the process queue associated with p->p_semAdd
 * 
 * @param p: the PCB to remove
 * @return pcb_t *: the removed PCB, or NULL if p->p_semAdd is NULL
**************************************************************/

pcb_PTR outBlocked (pcb_PTR p) {
    semd_t *prev, *curr;
    
    if (p == NULL || p->p_semAdd == NULL)
        return NULL;
    
    /* Locate the semaphore descriptor for p->p_semAdd */
    curr = getSemd(p->p_semAdd, &prev);
    if (curr == NULL || ((unsigned long) curr->s_semAdd != (unsigned long) p->p_semAdd))
        return NULL;
    
    /* Remove p from the process queue */
    pcb_t *removed = outProcQ(&(curr->s_procQ), p);
    if (removed == NULL) {
        return NULL;
    }
    
    /* If the process queue is now empty, remove the descriptor from the ASL */
    if (emptyProcQ(curr->s_procQ)) {
        if (prev != NULL)
            prev->s_next = curr->s_next;
        freeSemd(curr); /* Return the descriptor to the free list */
    }
    
    return removed;
}

/**************************************************************
 * headBlocked
 *
 * Returns the PCB at the head of the process queue associated with semaphore semAdd.
 *
 * @param semAdd: the semaphore address
 * @return pcb_t *: the head of the process queue 
 **************************************************************/

pcb_t *headBlocked (int *semAdd) {
    semd_t *prev, *curr;
    
    curr = getSemd(semAdd, &prev);
    if (curr == NULL || ((unsigned long) curr->s_semAdd != (unsigned long) semAdd)) {
        return NULL;
    }
    
    return headProcQ(curr->s_procQ);
}

