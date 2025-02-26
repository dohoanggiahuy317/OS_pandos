#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "../h/scheduler.h"
#include "../h/interrupts.h"
#include "../h/exceptions.h"
#include "../h/initial.h"
#include "/usr/include/umps3/umps/libumps.h"


/*********************************************************************************************
 * findInterruptDevice
 * 
 * @brief
 * function is used to find the device that generated the interrupt.
 * The function takes the interrupt line number as an argument and returns the device 
 * number with highest priority (smallest number).
 *
 * @protocol
 * This function first retrieve the Device Interrupt area that is saved in address 0x10000000.
 * Using the interrupt line number, it accesses the corresponding device bit map.
 * There are 5 lines of interrupts, indexing from 3 to 7, so we subtract the BASE_LINE from the line number. 
 * It then checks the status of each device on the specified interrupt line by using bitwise operations.
 * 
 * @note
 * Bitmap:
 * is a 32-bit value that represents the status of interrupts for several devices.
 * Each bit in the bitMap corresponds to a specific device
 * It collects the interrupt status into a single unsigned integer.
 * Since the devices are checked in order (device 0 first, then device 1, etc.), 
 * the bitMap lets the system easily determine the highest-priority device that needs service.
 * 
 * devregarea_t:
 * This structure represents a memory-mapped Device Register Area,
 * which is essentially a "API" :)? of hardware memory locations used for device management and system control.
 * Casting devregarea_t * creates a structured view of the memory at RAMBASEADDR, which is 0x10000000
 * 
 * @param interrupt_line_number: the line number of the interrupt
 * @return int: the device number
*********************************************************************************************/

HIDDEN int findInterruptDevice(int interrupt_line_number){

    /* tells the compiler to interpret the memory starting at RAMBASEADDR as a structure of type devregarea_t */
    devregarea_t *device_register_area;
    device_register_area = (devregarea_t *) RAMBASEADDR;

    /* get the device bit map from the interrupt_dev address */
    memaddr device_bit_map = device_register_area->interrupt_dev[interrupt_line_number - BASE_LINE];

    /* check the device bit map to find the device that generated the interrupt */
    if (device_bit_map & INTERRUPTS_CONST_DEVICE_0 != ALLOFF) {
        return DEVICE_0;
    } else if (device_bit_map & INTERRUPTS_CONST_DEVICE_1 != ALLOFF) {
        return DEVICE_1;
    } else if (device_bit_map & INTERRUPTS_CONST_DEVICE_2 != ALLOFF) {
        return DEVICE_2;
    } else if (device_bit_map & INTERRUPTS_CONST_DEVICE_3 != ALLOFF) {
        return DEVICE_3;
    } else if (device_bit_map & INTERRUPTS_CONST_DEVICE_4 != ALLOFF) {
        return DEVICE_4;
    } else if (device_bit_map & INTERRUPTS_CONST_DEVICE_5 != ALLOFF) {
        return DEVICE_5;
    } else if (device_bit_map & INTERRUPTS_CONST_DEVICE_6 != ALLOFF) {
        return DEVICE_6;
    }

    return DEVICE_7;
}

