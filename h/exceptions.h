#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include "../h/types.h"

extern void systemTrapHandler();
extern void tlbTrapHandler();
extern void programTrapHandler();
extern void addPigeonCurrentProcessHelper();
extern void uTLB_RefillHandler();


#endif