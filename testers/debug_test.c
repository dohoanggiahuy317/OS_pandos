/*
 * asl_debug_test.c
 *
 * A debug test program for the ASL module.
 *
 * This program mimics the original ASL test portion of p1test.c:
 *  - It initializes the ASL.
 *  - It performs multiple insertBlocked calls using different semaphore keys.
 *  - It then tests removeBlocked, headBlocked, and outBlocked.
 *
 * Compile with the other modules (pcb.c, asl.c) so that the ASL
 * functions are linked in.
 */

#include <stdio.h>
#include <stdlib.h>
#include "../h/const.h"
#include "../h/types.h"
#include "../h/pcb.h"
#include "../h/asl.h"

/* For consistency with the original test file */
#define MAXPROC 20
#define MAXSEM  MAXPROC

/* Global arrays used in the test */
pcb_t *procp[MAXPROC];
int sem[MAXSEM];   /* Each element is used as the semaphore key */
int onesem;        /* An extra semaphore key for a special test */

void error_exit(const char *msg) {
    fprintf(stderr, "ERROR: %s\n", msg);
    exit(1);
}

int main(void) {
    int i;
    pcb_t *p, *q;

    printf("==== ASL Debug Test Start ====\n\n");

    /* --- Initialize the ASL --- */
    initASL();
    printf("Initialized active semaphore list.\n\n");

    /*
     * --- Test insertBlocked ---
     *
     * First, for indices 10..MAXPROC-1, allocate a PCB and insert it
     * into the ASL with semaphore key sem[i].
     */
    printf("Running insertBlocked test #1...\n");
    for (i = 10; i < MAXPROC; i++) {
        procp[i] = allocPcb();
        if (procp[i] == NULL)
            error_exit("allocPcb returned NULL in test #1.");
        if (insertBlocked(&sem[i], procp[i]) != 0)
            error_exit("insertBlocked(1): unexpected error.");
    }
    printf("insertBlocked test #1 passed.\n\n");

    /*
     * Now, for indices 0..9, allocate another set of PCBs and insert them
     * with semaphore key sem[i].
     */
    printf("Running insertBlocked test #2...\n");
    for (i = 0; i < 10; i++) {
        procp[i] = allocPcb();
        if (procp[i] == NULL)
            error_exit("allocPcb returned NULL in test #2.");
        if (insertBlocked(&sem[i], procp[i]) != 0)
            error_exit("insertBlocked(2): unexpected error.");
    }
    printf("insertBlocked test #2 passed.\n\n");

    /*
     * --- Test descriptor return ---
     *
     * Remove one blocked process from semaphore sem[11] and reinsert it.
     * This should cause the semaphore descriptor (if its process queue becomes
     * empty) to be returned to the free list.
     */
    p = removeBlocked(&sem[11]);
    if (p == NULL)
        error_exit("removeBlocked failed to remove a process from sem[11].");
    if (insertBlocked(&sem[11], p) != 0)
        error_exit("insertBlocked on sem[11] failed to reuse descriptor.");
    printf("Descriptor return test passed.\n\n");

    /*
     * --- Test error on over-insertion ---
     *
     * If we try to insert using semaphore key 'onesem' (unused) with a PCB,
     * we expect failure (nonzero return) if that insertion would exceed our
     * allowed number of active semaphore descriptors.
     */
    if (insertBlocked(&onesem, procp[9]) == 0)
        error_exit("insertBlocked: inserted more than allowed (over-insertion test).");
    printf("Over-insertion test passed.\n\n");

    /*
     * --- Test removeBlocked and re-insertion ---
     *
     * For indices 10..MAXPROC-1, remove the blocked process and verify it
     * matches the one inserted earlier. Then reinsert it at semaphore key sem[i-10].
     */
    printf("Running removeBlocked test...\n");
    for (i = 10; i < MAXPROC; i++) {
        q = removeBlocked(&sem[i]);
        if (q == NULL)
            error_exit("removeBlocked: did not remove a process.");
        if (q != procp[i])
            error_exit("removeBlocked: removed wrong process.");
        if (insertBlocked(&sem[i - 10], q) != 0)
            error_exit("insertBlocked(3): unexpected error during re-insertion.");
    }
    if (removeBlocked(&sem[11]) != NULL)
        error_exit("removeBlocked: removed process from a nonexistent queue.");
    printf("removeBlocked/reinsertion test passed.\n\n");

    /*
     * --- Test headBlocked and outBlocked ---
     *
     * For semaphore sem[11], there should be no blocked process.
     * For sem[9], test that headBlocked returns the correct PCB,
     * then remove it with outBlocked, then repeat for the next PCB.
     */
    if (headBlocked(&sem[11]) != NULL)
        error_exit("headBlocked: non-NULL returned for a nonexistent queue.");

    q = headBlocked(&sem[9]);
    if (q == NULL)
        error_exit("headBlocked(1): returned NULL for an existing queue.");
    if (q != procp[9])
        error_exit("headBlocked(1): returned wrong process for sem[9].");

    p = outBlocked(q);
    if (p != q)
        error_exit("outBlocked(1): failed to remove the correct process.");
    
    /* Now the head of sem[9] should be the next blocked process. */
    q = headBlocked(&sem[9]);
    if (q == NULL)
        error_exit("headBlocked(2): returned NULL after outBlocked.");
    if (q != procp[19])
        error_exit("headBlocked(2): returned wrong process after outBlocked.");

    p = outBlocked(q);
    if (p != q)
        error_exit("outBlocked(2): failed to remove the correct process.");

    /* A subsequent outBlocked call on the same element should return NULL */
    p = outBlocked(q);
    if (p != NULL)
        error_exit("outBlocked: removed the same process twice.");

    if (headBlocked(&sem[9]) != NULL)
        error_exit("headBlocked: expected empty queue for sem[9].");

    printf("headBlocked and outBlocked tests passed.\n\n");
    printf("==== ASL Module Tests PASSED ====\n");

    return 0;
}