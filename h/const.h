#ifndef CONSTS
#define CONSTS

/**************************************************************************** 
 *
 * This header file contains utility constants & macro definitions.
 * 
 ****************************************************************************/

/* Hardware & software constants */
#define PAGESIZE		  4096			/* page size in bytes	*/
#define WORDLEN			  4				  /* word size in bytes	*/


/* timer, timescale, TOD-LO and other bus regs */
#define RAMBASEADDR		0x10000000
#define RAMBASESIZE		0x10000004
#define TODLOADDR		  0x1000001C
#define INTERVALTMR		0x10000020	
#define TIMESCALEADDR	0x10000024


/* utility constants */
#define	TRUE			    1
#define	FALSE			    0
#define HIDDEN			  static
#define EOS				    '\0'

#define PSECOND    100000	/* 100 msec */

#define NULL 			    ((void *)0xFFFFFFFF)

/* device interrupts */
#define DISKINT			  3
#define FLASHINT 		  4
#define NETWINT 		  5
#define PRNTINT 		  6
#define TERMINT			  7

#define DEVINTNUM		  5		  /* interrupt lines used by devices */
#define DEVPERINT		  8		  /* devices per interrupt line */
#define DEVREGLEN		  4		  /* device register field length in bytes, and regs per dev */	
#define DEVREGSIZE	  16 		/* device register size in bytes */

/* device register field number for non-terminal devices */
#define STATUS			  0
#define COMMAND			  1
#define DATA0			    2
#define DATA1			    3

/* device register field number for terminal devices */
#define RECVSTATUS  	0
#define RECVCOMMAND 	1
#define TRANSTATUS  	2
#define TRANCOMMAND 	3

/* device common STATUS codes */
#define UNINSTALLED		    0
#define READY			    1
#define BUSY			    3

/* device common COMMAND codes */
#define RESET			    0
#define ACK				    1

/* Memory related constants */
#define KSEG0           0x00000000
#define KSEG1           0x20000000
#define KSEG2           0x40000000
#define KUSEG           0x80000000
#define RAMSTART        0x20000000
#define BIOSDATAPAGE    0x0FFFF000
#define	PASSUPVECTOR	0x0FFFF900

/* Memory Constants */
#define UPROCSTARTADDR 0x800000B0
#define USERSTACKTOP   0xC0000000
#define KERNELSTACK    0x20001000

/* Exceptions related constants */
#define	PGFAULTEXCEPT	  0
#define GENERALEXCEPT	  1


/* operations */
#define	MIN(A,B)		((A) < (B) ? A : B)
#define MAX(A,B)		((A) < (B) ? B : A)
#define	ALIGNED(A)		(((unsigned)A & 0x3) == 0)

/* Macro to load the Interval Timer */
#define LDIT(T)	((* ((cpu_t *) INTERVALTMR)) = (T) * (* ((cpu_t *) TIMESCALEADDR))) 

/* Macro to read the TOD clock */
#define STCK(T) ((T) = ((* ((cpu_t *) TODLOADDR)) / (* ((cpu_t *) TIMESCALEADDR))))

/* Processor State--Status register constants */
#define ALLOFF			0x0     	/* every bit in the Status register is set to 0; this will prove helpful for bitwise-OR operations */
#define USERPON			0x00000008	/* constant for setting the user-mode on after LDST (i.e., KUp (bit 3) = 1) */
#define IEPON			0x00000004	/* constant for enabling interrupts after LDST (i.e., IEp (bit 2) = 1) */
#define IECON			0x00000001	/* constant for enabling the global interrupt bit (i.e., IEc (bit 0) = 1) */
#define PLTON			0x08000000	/* constant for enabling PLT (i.e., TE (bit 27) = 1) */
#define IMON			0x0000FF00	/* constant for setting the Interrupt Mask bits to on so interrupts are fully enabled */
#define	IECOFF			0xFFFFFFFE	/* constant for disabling the global interrupt bit (i.e., IEc (bit 0) = 0) */

/* Device Constant, Line constants, and Interrupt device bit map */
#define PLT_INTERRUPT_STATUS                0x00000002
#define INTERVAL_TIMER_INTERRUPT_STATUS     0x00000004
#define DEVICE_INTERRUPT_STATUS             0x00000008

#define INTERRUPTS_BIT_CONST_DEVICE_0       0x00000001
#define INTERRUPTS_BIT_CONST_DEVICE_1       0x00000002
#define INTERRUPTS_BIT_CONST_DEVICE_2       0x00000004
#define INTERRUPTS_BIT_CONST_DEVICE_3       0x00000008
#define INTERRUPTS_BIT_CONST_DEVICE_4       0x00000010
#define INTERRUPTS_BIT_CONST_DEVICE_5       0x00000020
#define INTERRUPTS_BIT_CONST_DEVICE_6       0x00000040
#define INTERRUPTS_BIT_CONST_DEVICE_7       0x00000080

#define DEVICE_0    0
#define DEVICE_1    1
#define DEVICE_2    2
#define DEVICE_3    3
#define DEVICE_4    4
#define DEVICE_5    5
#define DEVICE_6    6
#define DEVICE_7    7

#define	LINE1			1
#define	LINE2			2
#define	LINE3			3
#define	LINE4			4
#define	LINE5			5
#define	LINE6			6
#define	LINE7			7

/* SYSCALL constant */
#define	SYS1_NUM			1
#define	SYS2_NUM			2
#define	SYS3_NUM			3
#define	SYS4_NUM			4
#define	SYS5_NUM			5
#define	SYS6_NUM			6
#define	SYS7_NUM			7
#define	SYS8_NUM			8

/* Cause register BIT MASK constants */
#define CAUSE_INT_MASK                  0x1F
#define CAUSE_INT_SHIFT                 8
#define A8_BITS_ON                      0xFF
#define EXC_RESERVED_INSTRUCTION        10  
#define EXC_CODE_SHIFT                  2 
#define EXC_CODE_MASK                   0x0000007C
#define IP_LINE1_TIMER_BIT              0x00000200
#define IP_LINE2_TIMER_BIT              0x00000400

/* Clock Constants*/
#define PLT_TIME_SLICE          5000
#define INTERVAL_TIMER          100000
#define INF_TIME		        0xFFFFFFFF

/* Device Constants */
#define MAX_DEVICE_COUNT        49
#define CLOCK_INDEX             (MAX_DEVICE_COUNT - 1)
#define BASE_LINE               3

/* Process Constants */
#define	INIT_PROCESS_CNT		0
#define	INIT_SOFT_BLOCK_CNT		0
#define	PROCESS_INIT_START		0
#define MAXPROC 20
#define MAXINT 0x0FFFFFFF
#define MAXPROC_SEM (MAXPROC + 2)

/* Semaphore Constants */
#define SUCCESS_CONST		0
#define ERROR_CONST			-1


#endif
