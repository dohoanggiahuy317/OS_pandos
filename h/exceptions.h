#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include "../h/const.h"
#include "../h/types.h"
#include "../h/pcb.h"
#include "../h/asl.h"


extern void exceptionTrapHandler();
extern void uTLB_RefillHandler();

void addPigeonCurrentProcessHelper();

#endif