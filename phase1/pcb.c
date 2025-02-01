#include "../h/pcb.h"

static pcb_t pcbFree_list[MAXPROC];
static struct list_head pcbFree_h;



void initPcbs() {
    __init_list_head(&pcbFree_h);
};


void freePcb(pcb_t *p) {

};