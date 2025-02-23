/***********************************************************************************************
 * Process Control Block Implementation
 * --------------------------------------
 * This module implements and manages the Process Control Block abstraction,
 * which is essential for tracking processes in an operating system.
 *
 * Data Structures:
 * - pcb_t: Represents a Process Control Block containing:
 *   - p_next, p_prev: Pointers for maintaining a doubly linked list (used in free list and process queues)
 *   - p_prnt, p_child, p_lSib, p_rSib: Pointers for managing the process tree structure
 *   - p_s: The process state
 *   - p_time: The CPU time used by the process
 *   - p_semAdd: Pointer to the semaphore the process is blocked on
 *
 * - pcbFree_h: The head of the free list containing unused PCBs
 * - Process Queue: A circular doubly linked list structure used for managing ready and blocked processes
 * 
***********************************************************************************************/

#include "../h/pcb.h"
#include "../h/const.h"
#include "../h/types.h"

/* ------------------------------------------------------ */
/* ------------------ Global Variables ------------------ */
/* ------------------------------------------------------ */

static pcb_PTR pcbFree_h;

/* ----------------------------------------------------------------------- */
/* ------------------ Allocation/Deallocation Functions ------------------ */
/* ----------------------------------------------------------------------- */

/**************************************************************
 * initPcbs
 *
 * Initialize the pcbFree list to contain all the 
 * elements of the static array of MAXPROC pcbs
 * 
 * @param void
 * @return void
**************************************************************/

void initPcbs () {
    static pcb_t pcbFreeTable[MAXPROC];
    pcbFree_h = NULL;  /* The free list is empty at first */
    
    int i; /* Loop counter */
    for (i = 0; i < MAXPROC; i++) {
        /* For each pcb in the static array, insert it into the free list */

        freePcb(&pcbFreeTable[i]); /* add pcb to the free list */
    }
}

/**************************************************************
 * allocPcb
 *
 * Allocate a pcb from the pcbFree list
 * 
 * @param void
 * @return: a pointer to the allocated pcb, or NULL if no pcbs are available
**************************************************************/

pcb_PTR allocPcb () {
    pcb_t *p;

    if (pcbFree_h == NULL) {
        /* The free list is empty */
        return NULL;  
    }

    /* Remove one pcb from the free list */
    p = pcbFree_h;
    pcbFree_h = pcbFree_h->p_next;

    /* Initialize all fields of the pcb to default values */
    p->p_next    = NULL;
    p->p_prev    = NULL;
    p->p_prnt    = NULL;
    p->p_child   = NULL;
    p->p_lSib    = NULL;
    p->p_rSib    = NULL;
    
    /* Clear the process state */
    int j;
    for (j = 0; j < STATEREGNUM; j++) {
        p->p_s.s_reg[j] = 0;
    }
    

    /* Initialize other pcb fields */
    p->p_time   = 0;
    p->p_semAdd = NULL;

    p->p_supportStruct = NULL;

    return p;
}

/**************************************************************
 * freePcb
 *
 * Return a no-longer-in-use pcb to the free list
 * The pcb is inserted at the head of the list
 * 
 * @param p: the pcb to free
 * @return: void
**************************************************************/

void freePcb (pcb_PTR p) {
    if (p == NULL) { 
        /* nothing to free */
        return;  
    }


    if (pcbFree_h == NULL) {
        /* The free list is empty */
        p->p_next = NULL;
        p->p_prev = NULL;
        pcbFree_h = p;
        return;
    } 

    /* Insert p at the head of the free list */
    p->p_next = pcbFree_h;
    p->p_prev = NULL;
    pcbFree_h->p_prev = p;
    pcbFree_h = p;
    return;
}


/* ---------------------------------------------------------------- */
/* ------------------ Process Queue Maintainance ------------------ */
/* ---------------------------------------------------------------- */


/**************************************************************
 * mkEmptyProcQ
 *
 * Initialize a process queue to be empty.
 * An empty queue is represented by a NULL tail pointer
**************************************************************/

pcb_PTR mkEmptyProcQ () {
    return NULL;
}

/**************************************************************
 * emptyProcQ
 *
 * Return TRUE if the process queue whose 
 * tail pointer is tp is empty and FALSE otherwise.
 * 
 * @param tp: the tail pointer of the process queue
 * @return: 1 if the queue is empty, 0 otherwise
**************************************************************/

int emptyProcQ (pcb_PTR tp) {
    return (tp == NULL);
}

/************************************************************** 
 * insertProcQ
 *
 * Insert the pcb pointed to by p into the process queue whose tail pointer
 * is pointed to by tp. This function updates the tail pointer if necessary.
**************************************************************/

void insertProcQ (pcb_PTR *tp, pcb_PTR p) {
    pcb_t *tail, *head;
    
    if (p == NULL) {
        return;
    }
    
    
    if (*tp == NULL) {
        /* If the queue is empty then p becomes the only element. */
        p->p_next = p;
        p->p_prev = p;
        *tp = p;
    } else {
        /* Queue is non-empty. Let tail be *tp and head be tail->p_next */
        tail = *tp;
        head = tail->p_next;
        tail->p_next = p;
        p->p_prev = tail;
        p->p_next = head;
        head->p_prev = p;
        
        *tp = p; /* p becomes the new tail */
    }
}
/**************************************************************
 * removeProcQ
 *
 * Remove and return the first element from the process queue
 * Update the tail pointer if needed
 * If the queue is empty, return NULL
 * 
 * @param tp: the tail pointer of the process queue
 * @return: the removed pcb, or NULL if the queue is empty
**************************************************************/

pcb_PTR removeProcQ (pcb_PTR *tp) {
    pcb_t *head, *tail;

    if (tp == NULL || *tp == NULL) {
        return NULL;
    }
    
    tail = *tp;
    head = tail->p_next;
    
    if (head == tail) {
        /* Only one element in the queue */
        *tp = NULL;
    } else {
        /* Remove the head element */
        tail->p_next = head->p_next;
        head->p_next->p_prev = tail;
    }
    
    /* Clear the removed pointer */
    head->p_next = NULL;
    head->p_prev = NULL;
    return head;
}

/**************************************************************
 * outProcQ
 *
 * Remove the pcb pointed to by p from the process queue
 * If p is not found, return NULL, otherwise, return p
 * Update the tail pointer if necessary
 * 
 * @param tp: the tail pointer of the process queue
 * @param p: the pcb to remove
 * @return: the removed pcb, or NULL if p is not in the queue
**************************************************************/

pcb_PTR outProcQ (pcb_PTR *tp, pcb_PTR p) {
    pcb_t *current, *tail;

    if (tp == NULL || *tp == NULL || p == NULL) {
        return NULL;
    }
    
    tail = *tp;
    current = tail->p_next; /* start at the head */

    /* Loop over the circular queue until return to the head */
    do {
        if (current == p) { 
            /* Found the element to remove */
            
            if (current->p_next == current) { 
                /* p is the only element in the queue */
                *tp = NULL;
            } else {
                /* Remove current from the list */
                current->p_prev->p_next = current->p_next;
                current->p_next->p_prev = current->p_prev;
                if (current == *tp) {  /* if p is the tail, update tail pointer */
                    *tp = current->p_prev;
                }
            }
            /* Clear removed pcb pointers */
            p->p_next = NULL;
            p->p_prev = NULL;
            return p;
        }
        current = current->p_next;
    } while (current != tail->p_next);

    return NULL; /* p was not found in the queue */
}

/**************************************************************
 * headProcQ
 *
 * Return the head of the process queue 
 * Return NULL if the queue is empty.
 * 
 * @param tp: the tail pointer of the process queue
 * @return: the head of the queue, or NULL if the queue is empty
**************************************************************/

pcb_PTR headProcQ (pcb_PTR tp) {
    if (tp == NULL) {
        return NULL;
    }
    return tp->p_next;
}


/* ---------------------------------------------------------------- */
/* ------------------ Process Queue Maintainance ------------------ */
/* ---------------------------------------------------------------- */


/**************************************************************
 * emptyChild
 *
 * Return TRUE if the pcb has no children, FALSE otherwise.
 * 
 * @param p: the pcb to check
 * @return: 1 if the pcb has no children, 0 otherwise
**************************************************************/

int emptyChild (pcb_PTR p) {
    if (p == NULL || p->p_child == NULL) {
        return 1;  /* treat a null pcb as having no children */
    }
    return (p->p_child == NULL);
}

/**************************************************************
 * insertChild
 *
 * Make a child of the pcb pointed to by prnt.
 * Insert p at the beginning of prnt's child list.
 * 
 * @param prnt: the parent pcb
 * @param p: the child pcb
**************************************************************/

void insertChild (pcb_PTR prnt, pcb_PTR p) {
    if (prnt == NULL || p == NULL) {
        return;
    }

    if (prnt->p_child == NULL) {
        /* prnt has no children */
        prnt->p_child = p;
        p->p_prnt = prnt;
        p->p_lSib = NULL;
        p->p_rSib = NULL;
        return;
    }

    p->p_prnt = prnt;
    p->p_lSib = NULL;
    p->p_rSib = prnt->p_child;
    prnt->p_child->p_lSib = p;
    prnt->p_child = p;
    return;
}

/**************************************************************
 * removeChild
 *
 * Remove and return the first child of the pcb pointed to by p
 * If p has no children, return NULL
 * 
 * @param p: the parent pcb
 * @return: the removed child, or NULL if p has no children
**************************************************************/

pcb_PTR removeChild (pcb_PTR p) {
    pcb_t *child;
    
    if (p == NULL || p->p_child == NULL) {
        return NULL;
    }
    
    /* Remove the first child */
    child = p->p_child;
    p->p_child = child->p_rSib;

    /* Remove the reference from the removed child */
    child->p_prnt = NULL;
    child->p_rSib = NULL;
    child->p_lSib = NULL;
    return child;
}


/**************************************************************
 * outChild
 *
 * Remove the pcb from the child list of its parent
 * If p has no parent, return NULL; otherwise, return p
 * 
 * @param p: the pcb to remove
 * @return: the removed pcb, or NULL if p has no parent
**************************************************************/

pcb_PTR outChild (pcb_PTR p) {
    pcb_t *prnt, *cur;
    
    if (p == NULL || p->p_prnt == NULL) {
        return NULL;
    }
    
    prnt = p->p_prnt;
    
    /* If p is the first child, update prnt->p_child directly */
    if (prnt->p_child == p) {
        removeChild(prnt);
    } else {
        /* Otherwise, search the child list for p */
        cur = prnt->p_child;
        while (cur != NULL && cur->p_rSib != p) {
            cur = cur->p_rSib;
        }

        if (cur != NULL) {
            cur->p_rSib = p->p_rSib;
            if (p->p_rSib != NULL) {
                ((pcb_PTR)p->p_rSib)->p_lSib = cur;
            }
        } else {
            /* p not found in parent's child list */
            return NULL;
        }
    }
    
    p->p_prnt = NULL;
    p->p_rSib = NULL;
    p->p_lSib = NULL;
    return p;
}