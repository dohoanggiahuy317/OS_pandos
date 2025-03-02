#ifndef PCB
#define PCB

/************************* PROCQ.H *****************************
*
*  The externals declaration file for the Process Control Block
*    Module.
*
*  Editted by JaWeee
*/

#include "../h/types.h"

/***************************************************************/
/* The Process Control Block                                   */
/***************************************************************/

extern void freePcb (pcb_PTR p);
extern pcb_PTR allocPcb ();
extern void initPcbs ();

/***************************************************************/
/* The Process Queue                                           */
/***************************************************************/

extern pcb_PTR mkEmptyProcQ ();
extern int emptyProcQ (pcb_PTR tp);
extern void insertProcQ (pcb_PTR *tp, pcb_PTR p);
extern pcb_PTR removeProcQ (pcb_PTR *tp);
extern pcb_PTR outProcQ (pcb_PTR *tp, pcb_PTR p);
extern pcb_PTR headProcQ (pcb_PTR tp);

/***************************************************************/
/* The Child Queue                                             */
/***************************************************************/

extern int emptyChild (pcb_PTR p);
extern void insertChild (pcb_PTR prnt, pcb_PTR p);
extern pcb_PTR removeChild (pcb_PTR p);
extern pcb_PTR outChild (pcb_PTR p);

/***************************************************************/

#endif
