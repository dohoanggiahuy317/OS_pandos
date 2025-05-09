#ifndef ASL
#define ASL

/************************** ASL.H ******************************
*
*  The externals declaration file for the Active Semaphore List
*    Module.
*
*  Editted by JaWeee
*/

#include "../h/types.h"

/***************************************************************/
/* The Semaphore Descriptor                                    */
/***************************************************************/

extern int insertBlocked (int *semAdd, pcb_PTR p);
extern pcb_PTR removeBlocked (int *semAdd);
extern pcb_PTR outBlocked (pcb_PTR p);
extern pcb_PTR headBlocked (int *semAdd);
extern void initASL ();

/***************************************************************/

#endif
