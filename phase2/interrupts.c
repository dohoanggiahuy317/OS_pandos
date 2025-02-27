/*********************************************************************************************
 * Interrupts.c
 * 
 * @brief
 * This file is generally responsible for managing and handling interrupts in a system. 
 * Interrupts are signals that cause the processor to temporarily halt its current task and 
 * jump to a dedicated function to handle events like hardware inputs, timers, or other asynchronous signals.
 * 
 * @def
 * interruptTrapHandler()
 * This is the main entry point for handling interrupts. It saves the current state,
 * determines the type of interrupt with priority examination, and calls the appropriate handler function.
 * 
 * nonTimerInterruptHandler(), PLTInterruptHandler(), intervalTimerInterruptHandler()
 * These are specific handlers for different types of interrupts.
 * nonTimerInterruptHandler() deals with device interrupts,
 * PLTInterruptHandler() manages the timer interrupt for process scheduling,
 * and intervalTimerInterruptHandler() handles the pseudo-clock interrupts.
 * Details about the purpose, use case, description, and protocol of each function are provided in the code comments.
 * 
 * @note
 * There are 2 important things to remember:
 * 1. After handling the interrupt, the current process brings the exception state back (that's why I use addPigeonCurrentProcessHelper()
 * because I feel like it is a pigeon that brings the exception state back) to notify the system that the interrupt is handled
 * 2. The cpu_time of the interrupted is charged to the interrupting process, not the current process. 
 * What this file does is update the cpu_time of the current process by 
 * adding the cpu time of the current process with: (interrupt_TOD - start_TOD) -> time from the current process 
 * to the interrupt start handler
 * 
 * @author
 * JaWeee Do
*********************************************************************************************/


#include "../h/types.h"
#include "../h/const.h"
#include "../h/initial.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../h/scheduler.h"
#include "../h/pcb.h"
#include "../h/asl.h"
#include "/usr/include/umps3/umps/libumps.h"


/* ---------------------------------------------------------------------------------------------- */
/* ------------------------------------------ Variables ----------------------------------------- */
/* ---------------------------------------------------------------------------------------------- */
cpu_t current_process_time_left; /* Calculate the remaining time in the time slice of the current process */
cpu_t interrupt_TOD; /* Record the time of the interrupt */

/* ---------------------------------------------------------------------------------------------- */
/* ------------------------------------------ INTERRUPT ----------------------------------------- */
/* ---------------------------------------------------------------------------------------------- */

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

int findInterruptDevice(int interrupt_line_number){

    /* tells the compiler to interpret the memory starting at RAMBASEADDR as a structure of type devregarea_t */
    devregarea_t *device_register_area;
    device_register_area = (devregarea_t *) RAMBASEADDR;

    /* get the device bit map from the interrupt_dev address */
    memaddr device_bit_map = device_register_area->interrupt_dev[interrupt_line_number - BASE_LINE];

    /* check the device bit map to find the device that generated the interrupt */
    if ( (device_bit_map & INTERRUPTS_BIT_CONST_DEVICE_0) != ALLOFF) {
        return DEVICE_0;
    } else if ( (device_bit_map & INTERRUPTS_BIT_CONST_DEVICE_1) != ALLOFF) {
        return DEVICE_1;
    } else if ((device_bit_map & INTERRUPTS_BIT_CONST_DEVICE_2) != ALLOFF) {
        return DEVICE_2;
    } else if ((device_bit_map & INTERRUPTS_BIT_CONST_DEVICE_3) != ALLOFF) {
        return DEVICE_3;
    } else if ((device_bit_map & INTERRUPTS_BIT_CONST_DEVICE_4) != ALLOFF) {
        return DEVICE_4;
    } else if ((device_bit_map & INTERRUPTS_BIT_CONST_DEVICE_5) != ALLOFF) {
        return DEVICE_5;
    } else if ((device_bit_map & INTERRUPTS_BIT_CONST_DEVICE_6) != ALLOFF) {
        return DEVICE_6;
    }

    return DEVICE_7;
}

/*********************************************************************************************
 * nonTimerInterruptHandler
 * 
 * @brief
 * This function is used to handle non-timer interrupts.
 * The function first calculates the device register address,
 * then checks the device status, acknowledges the interrupt, and performs the V operation.
 * Finally, it saves the status code to the newly unblocked process register v0 and inserts the PCB to the ready queue.
 * 
 * @protocol
 * 7 Step is descriped in the function
 * 
 * @param void
 * @return void
 ***********************************************************************************************/

void nonTimerInterruptHandler() {
    /* tells the compiler to interpret the memory starting at RAMBASEADDR as a structure of type devregarea_t */
    devregarea_t *device_register_area;
    device_register_area = (devregarea_t *) RAMBASEADDR;


    /**
     * STEP 1: Calculate the Device Register Address
     * 
     * @brief
     * What It Means:
     * Every I/O device has a specific location in memory called its device register.
     * This register is used to both read information (the status when the I/O return you smth it done)
     * and send commands (you use this to tell I/O that you got the information, please don't annoy).
     * 
     * @protocol
     * 1. The handler calculates the memory address based on the device’s known location in the system
     * Like a “mailbox” in memory holds the device’s information. 
     * 
     * 2. Get the interrupt line number from the Cause register
     * (Pandos page 18) IP (bits 8-15): an 8-bit field indicating on which interrupt lines interrupts
     * are currently pending. If an interrupt is pending on interrupt 
     * line i, then Cause.IP[i] is set to 1.
     *
     * 3. Shift the Cause register right by 8 bits to get the interrupt line number
     * 
     * 4. Find the device index in the device register area
     *      - Convert to base 0 index
     *      - Multiply number of devices per interrupt line.
     *      - Add the device number  
     */
    int interrupt_line_number = ((savedExceptionState->s_cause >> 11) & 0x1F) + 3;
    int device_num = findInterruptDevice(interrupt_line_number);

    /* get the device index from the interrupt line number (similar to semaphore index in exception.c)*/
    int device_index = (interrupt_line_number - BASE_LINE) * DEVPERINT + device_num; 


    /**
     * STEP 2 + 3 + 4: Check the Device Status, Acknowledge the Interrupt, Perfom the V Operation
     * 
     * Using the cause register of the device that we got the index above, read the status of the device
     * to understand the outcome of the request
     * Right now, the device is holding the hammer (interrupt) and knock on the OS head that it is done 
     * so we need to handle it and tell the I/O that we got the information, please stop annoying us
     * This is done by writing to the device register area acknowledge the interrupt and perform the V operation
     * This means the process that is waiting for the device to finish can continue
     * 
     * @protocol
     * 1. get the Status register of the device
     *      - if it is the terminal device, check if this is a write or read interrupt
     *        The t_transm_status in the register is NOT READY doesnt mean the I/O is not ready
     *        This means this is the write interrupt because
     *        it is not ready which means that variable in the past was need to update, 
     *        so it block on it and when the device done, it knock and tell me to take it
     *     - else, we just take the status code
     * 2. Acknowledge the interrupt (Pandos page 43)
     * 3. Perform the V operation
     *      - Unblock the process that is waiting for the device to finish
     *      - The process that is waiting for the device to finish can continue
     *      - Increase semaphore value by 1
     * 
     * @note
     * The transmit_status is an 8-bit value that indicates the status of the device
     * We need to mask it with A8_BITS_ON = 0xFF
     */

    int status_code;
    pcb_PTR pcb_to_unblock;
    if (
        (interrupt_line_number == LINE7) && 
        (((device_register_area->devreg[device_index].t_transm_status) & A8_BITS_ON) != READY)){
		    
            /* save the status code */
            status_code = device_register_area->devreg[device_index].t_transm_status;

            /* acknowledge the interrupt */
		    device_register_area->devreg[device_index].t_transm_command = ACK;

            /* perform the V operation */
            pcb_to_unblock = removeBlocked(&semaphoreDevices[device_index + DEVPERINT]);
		    semaphoreDevices[device_index + DEVPERINT]++;
	} else {
		status_code = device_register_area->devreg[device_index].t_recv_status;
		device_register_area->devreg[device_index].t_recv_status = ACK;
		pcb_to_unblock = removeBlocked(&semaphoreDevices[device_index]);
		semaphoreDevices[device_index]++;
	}

    /**
     * STEP 5 + 6: Save the status code to newly unblock process register v0 and insert the PCB to the ready queue
     * 
     * @brief
     * Save the status to register 0 of the PCB that is unblocked
     * Kick in the @$$ of the PCB to the ready queue because now the I/O is done
     * 
     * @protocol
     * 1. Save the status code to register v0
     * 2. Insert the PCB to the ready queue
     */

    if (pcb_to_unblock != NULL) {
        currentProcess->p_s.s_v0 = status_code;
        insertProcQ(&readyQueue, currentProcess);
        softBlockedCount--;
        STCK(curr_TOD);
        pcb_to_unblock->p_time = pcb_to_unblock->p_time + (curr_TOD - interrupt_TOD);
    }

    /** STEP 7: Switch Control to the current process
     * 
     * @brief
     * The handler performs an LDST (Load State) operation, which restores the saved exception state from the BIOS Data Page. 
     * This step effectively transfers control back to the process that was interrupted, now with the I/O completion handled.
     * 
     * @protocol
     * 1. add the exception state to the current process
     * 2. Set the timer to the current process time left
     * 3. Set the current process time to the current process time + (current time - interrupt time)
     * 4. Switch control to the current process
     */
    if (currentProcess != NULL) {
        addPigeonCurrentProcessHelper();
        setTIMER(current_process_time_left);
        updateProcessTimeHelper(currentProcess, start_TOD, interrupt_TOD);
        switchContext(currentProcess);
    }

    scheduler();
}   

/*********************************************************************************************
 * PLTInterruptHandler
 * 
 * @brief
 * This function is used to handle PLT interrupts. The PLT is used to support CPU scheduling.
 * The Scheduler will load the PLT with the value of 5 milliseconds (Pandos page 43)
 * 
 * @protocol
 * 1. Acknowledge the PLT interrupt by reloading the timer with 5 milliseconds.
 *   This tells the hardware that the interrupt has been handled.
 * 2. Copy the processor state from the BIOS Data Page (saved during the exception)
 *  into the current process's PCB.
 * 3. Update the accumulated CPU time for the current process.
 * Here we get the current time-of-day and add the elapsed time to the process's total.
 * 4. Transition the current process from the "running" state to the "ready" state
 * by inserting it into the ready queue.
 * 5. Call the scheduler to choose the next process to run.
 * 
 * @param void
 * @return void
 ***********************************************************************************************/

HIDDEN void PLTInterruptHandler() {
    /* Step 0: Get the current process, if there is no current process, panic */    
    if (currentProcess == NULL) {
        PANIC();
        return;
    }

    /* Step 1: Acknowledge the PLT interrupt by reloading the timer with 5 milliseconds. */
    setTIMER(PLT_TIME_SLICE);

    /* Step 2: Copy the processor state from the BIOS Data Page */
    addPigeonCurrentProcessHelper();

    /* Step  3: Update the accumulated CPU time for the current process */
    STCK(curr_TOD);
    updateProcessTimeHelper(currentProcess, start_TOD, curr_TOD);

    /* STEP 4: Transition the current process from the "running" state to the "ready" state */
    insertProcQ(&readyQueue, currentProcess);

    /*  STEP 5: call the scheduler */
    scheduler();
}

/*********************************************************************************************
 * intervalTimerInterruptHandler
 * 
 * @brief
 * This function is used to handle interval timer interrupts. 
 * Instead of each process having its own timer, the OS uses one timer to periodically 
 * wake up all processes waiting for time delays.
 * In a round-robin system, all ready processes are scheduled in a fair, cyclic order. 
 * So, when a process is unblocked by the pseudo-clock, it becomes eligible for the next round of scheduling
 * 
 * @protocol
 * 1. Acknowledge the interrupt by loading the Interval Timer with 100 milliseconds.
 * 2. Unblock all PCBs blocked on the Pseudo-clock semaphore
 * 3. Reset the Pseudo-clock semaphore to zero.
 * 4. Return control to the Current Process if one exists, otherwise call scheduler.
 * 
 * @note
 * Unblocked processes join the ready queue in a round-robin manner
 * it just makes them eligible to be scheduled again.
 * 
 * @param void
 * @return void
 ***********************************************************************************************/

HIDDEN void intervalTimerInterruptHandler() {
    pcb_PTR pcb_to_unblock;

    /* Step 1: Acknowledge the interrupt by loading the Interval Timer with 100 milliseconds. */
    setTIMER(INTERVAL_TIMER);

    /* Step 2: Unblock ALL pcbs blocked on the Pseudo-clock semaphore */
    while (headBlocked(&semaphoreDevices[CLOCK_INDEX]) != NULL) {
        pcb_to_unblock = removeBlocked(&semaphoreDevices[CLOCK_INDEX]);
        insertProcQ(&readyQueue, pcb_to_unblock);
        softBlockedCount--;
    }

    /* Step 3: Reset the Pseudo-clock semaphore to zero. */
    semaphoreDevices[CLOCK_INDEX] = 0;

    /* Step 4: Return control to the Current Process if one exists, otherwise call scheduler. */
    if (currentProcess != NULL) {
        setTIMER(current_process_time_left);
        addPigeonCurrentProcessHelper();
        updateProcessTimeHelper(currentProcess, start_TOD, interrupt_TOD);
        switchContext(currentProcess);
    }
    
    /* Step 5: Call the scheduler */
    scheduler();
}

/* ---------------------------------------------------------------------------------------------- */
/* ------------------------------------- HELPER + HANDLER --------------------------------------- */
/* ---------------------------------------------------------------------------------------------- */

/*********************************************************************************************
 * interruptTrapHandler
 * 
 * @brief
 * This function is used to handle interrupts. It is called when an interrupt occurs.
 * The function first saves the current time and timer value, then saves the exception state from the BIOS data page.
 * It then extracts the pending interrupt bits from the Cause register and checks for PLT, Interval Timer, and device interrupts.
 * If no known interrupt is pending, it simply calls the scheduler.
 * 
 * @protocol
 * 1. Save the current time and timer value
 * 2. Save the exception state from the BIOS data page
 * 3. Extract the pending interrupt bits from the Cause register
 * 4. Check for PLT, Interval Timer, and device interrupts
 * 5. If no known interrupt is pending, call the scheduler
 * 
 * @param void
 * @return void
 ***********************************************************************************************/

void interruptTrapHandler() {
    /* record current time and timer value */
    STCK(interrupt_TOD);
    current_process_time_left = getTIMER();

    /* save the exception state from the BIOS data page */
    savedExceptionState = (state_PTR) BIOSDATAPAGE;

    /* extract the pending interrupt bits (bits 8-15 of the Cause register) */
    unsigned int pending;
    pending = (savedExceptionState->s_cause >> 8) & 0xFF;
    pending &= 0xFE; /* ignore interrupt line 0 as per uniprocessor design */

    /* Check for PLT interrupt (interrupt line 1, highest priority) */
    if (pending & PLT_INTERRUPT_STATUS) {
        PLTInterruptHandler();
        return;
    }
    /* Check for pseudo-clock (Interval Timer) interrupt (interrupt line 2) */
    else if (pending & INTERVAL_TIMER_INTERRUPT_STATUS) {
        intervalTimerInterruptHandler();
        return;
    }
    /* Check for device interrupts on lines 3-7 */
    else if (pending & DEVICE_INTERRUPT_STATUS) {
        nonTimerInterruptHandler();
        return;
    }
    /* if no known interrupt pending, simply call scheduler */
    else {
        scheduler();
    }
}

