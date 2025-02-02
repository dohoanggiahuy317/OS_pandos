#include "../h/pcb.h"      
#include "../h/const.h" 
#include <string.h>  

/* ------------------------------------------------------ */
/* ------------------ Global Variables ------------------ */
/* ------------------------------------------------------ */

static pcb_t pcbFreeTable[MAXPROC];
static pcb_t *pcbFree_h = NULL;


/* ----------------------------------------------------------------------- */
/* ------------------ Allocation/Deallocation Functions ------------------ */
/* ----------------------------------------------------------------------- */

/**************************************************************
 * initPcbs
 *
 * Initialize the pcbFree list to contain all the elements of the static
 * array of MAXPROC pcbs. This method is called once during system startup.
 * 
 * @param void
 * @return void
**************************************************************/

void initPcbs () {
    int i;
    pcbFree_h = NULL;  /* initially, the free list is empty */
    
    /* For each pcb in the static array, insert it into the free list */
    for (i = 0; i < MAXPROC; i++) {
        /* add it to the free list */
        freePcb(&pcbFreeTable[i]);
    }
}

/**************************************************************
 * allocPcb
 *
 * Return a pointer to a free pcb after initializing all its fields.
 * If the free list is empty, return NULL.
**************************************************************/

pcb_PTR allocPcb () {
    pcb_t *p;

    if (pcbFree_h == NULL) {
        return NULL;  /* no free pcb available */
    }

    /* Remove one pcb from the free list */
    p = pcbFree_h;
    pcbFree_h = pcbFree_h->p_next;

    /* Initialize all fields of the pcb to default values */
    p->p_next    = NULL;
    p->p_prev    = NULL;
    p->p_prnt    = NULL;
    p->p_child   = NULL;
    p->p_sib     = NULL;
    
    /* Clear the process state.
     * Here we assume that state_t fields should be 0.
     */
    p->p_s.s_entryHI = 0;
    p->p_s.s_cause   = 0;
    p->p_s.s_status  = 0;
    p->p_s.s_pc      = 0;
    {
        int j;
        for (j = 0; j < STATEREGNUM; j++) {
            p->p_s.s_reg[j] = 0;
        }
    }

    /* Initialize other pcb fields */
    p->p_time   = 0;
    p->p_semAdd = NULL;

    /* p->p_supportStruct IGNOREEEEE */

    return p;
}

/**************************************************************
 * freePcb
 *
 * Return a pcb (which is no longer in use) to the free list.
 * The pcb is inserted at the head of the list.
 * 
 * @param p: the pcb to free
 * @return: void
**************************************************************/

void freePcb (pcb_PTR p) {
    if (p == NULL) {
        return;  /* nothing to free */
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








/******************** Process Queue (ProcQ) Functions ********************/

/*
 * mkEmptyProcQ
 *
 * Initialize a process queue (which is maintained as a double-circular list)
 * by returning a tail pointer to an empty queue. (An empty queue is represented
 * by a NULL tail pointer.)
 */
pcb_t *mkEmptyProcQ () {
    return NULL;
}

/*
 * emptyProcQ
 *
 * Return TRUE if the process queue whose tail pointer is tp is empty,
 * FALSE otherwise.
 */
int emptyProcQ (pcb_t *tp) {
    return (tp == NULL);
}

/*
 * insertProcQ
 *
 * Insert the pcb pointed to by p into the process queue whose tail pointer
 * is pointed to by tp. This function updates the tail pointer if necessary.
 */
void insertProcQ (pcb_t **tp, pcb_t *p) {
    pcb_t *tail, *head;
    
    if (p == NULL) {
        return;
    }
    
    /* If the queue is empty, p becomes the only element. */
    if (*tp == NULL) {
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
        *tp = p;  /* p becomes the new tail */
    }
}

/*
 * removeProcQ
 *
 * Remove and return the first element (the head) from the process queue
 * whose tail pointer is pointed to by tp. Update the tail pointer if needed.
 * If the queue is empty, return NULL.
 */
pcb_t *removeProcQ (pcb_t **tp) {
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
    
    /* Clear the removed element's pointers */
    head->p_next = NULL;
    head->p_prev = NULL;
    return head;
}

/*
 * outProcQ
 *
 * Remove the pcb pointed to by p from the process queue whose tail pointer
 * is pointed to by tp. If p is not found in the queue, return NULL;
 * otherwise, return p. Update the tail pointer if necessary.
 */
pcb_t *outProcQ (pcb_t **tp, pcb_t *p) {
    pcb_t *current, *tail;

    if (tp == NULL || *tp == NULL || p == NULL) {
        return NULL;
    }
    
    tail = *tp;
    current = tail->p_next; /* start at the head */

    /* Loop over the circular queue until we return to the head */
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
            /* Clear removed pcb's queue pointers */
            p->p_next = NULL;
            p->p_prev = NULL;
            return p;
        }
        current = current->p_next;
    } while (current != tail->p_next);

    /* p was not found in the queue */
    return NULL;
}

/*
 * headProcQ
 *
 * Return the head of the process queue (the first element) whose tail pointer
 * is tp. Do not remove the element. Return NULL if the queue is empty.
 */
pcb_t *headProcQ (pcb_t *tp) {
    if (tp == NULL) {
        return NULL;
    }
    return tp->p_next;
}


/******************** Process Tree Functions ********************/

/*
 * emptyChild
 *
 * Return TRUE if the pcb pointed to by p has no children; FALSE otherwise.
 */
int emptyChild (pcb_t *p) {
    if (p == NULL) {
        return 1;  /* treat a null pcb as having no children */
    }
    return (p->p_child == NULL);
}

/*
 * insertChild
 *
 * Make the pcb pointed to by p a child of the pcb pointed to by prnt.
 * Insert p at the beginning of prnt's child list.
 */
void insertChild (pcb_t *prnt, pcb_t *p) {
    if (prnt == NULL || p == NULL) {
        return;
    }
    p->p_prnt = prnt;
    /* Insert p at the front of the child list */
    p->p_sib = prnt->p_child;
    prnt->p_child = p;
}

/*
 * removeChild
 *
 * Remove and return the first child of the pcb pointed to by p.
 * If p has no children, return NULL.
 */
pcb_t *removeChild (pcb_t *p) {
    pcb_t *child;
    
    if (p == NULL || p->p_child == NULL) {
        return NULL;
    }
    
    child = p->p_child;
    p->p_child = child->p_sib;
    
    /* Remove the parent's reference from the removed child */
    child->p_prnt = NULL;
    child->p_sib = NULL;
    return child;
}

/*
 * outChild
 *
 * Remove the pcb pointed to by p from the child list of its parent.
 * If p has no parent, return NULL; otherwise, return p.
 * p can be any child in the list.
 */
pcb_t *outChild (pcb_t *p) {
    pcb_t *prnt, *cur;
    
    if (p == NULL || p->p_prnt == NULL) {
        return NULL;
    }
    
    prnt = p->p_prnt;
    
    /* If p is the first child, update prnt->p_child directly */
    if (prnt->p_child == p) {
        prnt->p_child = p->p_sib;
    } else {
        /* Otherwise, search the child list for p */
        cur = prnt->p_child;
        while (cur != NULL && cur->p_sib != p) {
            cur = cur->p_sib;
        }
        if (cur != NULL) {
            cur->p_sib = p->p_sib;
        } else {
            /* p not found in parent's child list (should not happen) */
            return NULL;
        }
    }
    
    p->p_prnt = NULL;
    p->p_sib = NULL;
    return p;
}