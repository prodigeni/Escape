/**
 * @version		$Id$
 * @author		Nils Asmussen <nils@script-solution.de>
 * @copyright	2008 Nils Asmussen
 */

#include "../h/common.h"
#include "../h/intrpt.h"
#include "../h/util.h"
#include "../h/keyboard.h"
#include "../h/cpu.h"
#include "../h/paging.h"
#include "../h/proc.h"
#include "../h/elf.h"
#include "../h/syscalls.h"
#include "../h/vfs.h"
#include "../h/vfsreal.h"
#include "../h/gdt.h"
#include "../h/kheap.h"
#include "../h/video.h"
#include "../h/sched.h"
#include "../h/signals.h"
#include <string.h>
#include <sllist.h>

#define IDT_COUNT		256
/* the privilege level */
#define IDT_DPL_KERNEL	0
#define IDT_DPL_USER	3
/* reserved by intel */
#define IDT_INTEL_RES1	2
#define IDT_INTEL_RES2	15
/* the code-selector */
#define IDT_CODE_SEL	0x8

/* I/O ports for PICs */
#define PIC_MASTER		0x20				/* base-port for master PIC */
#define PIC_SLAVE		0xA0				/* base-port for slave PIC */
#define PIC_MASTER_CMD	PIC_MASTER			/* command-port for master PIC */
#define PIC_MASTER_DATA	(PIC_MASTER + 1)	/* data-port for master PIC */
#define PIC_SLAVE_CMD	PIC_SLAVE			/* command-port for slave PIC */
#define PIC_SLAVE_DATA	(PIC_SLAVE + 1)		/* data-port for slave PIC */

#define PIC_EOI			0x20				/* end of interrupt */

/* flags in Initialization Command Word 1 (ICW1) */
#define ICW1_NEED_ICW4	0x01				/* ICW4 needed */
#define ICW1_SINGLE		0x02				/* Single (not cascade) mode */
#define ICW1_INTERVAL4	0x04				/* Call address interval 4 (instead of 8) */
#define ICW1_LEVEL		0x08				/* Level triggered (not edge) mode */
#define ICW1_INIT		0x10				/* Initialization - required! */

/* flags in Initialization Command Word 4 (ICW4) */
#define ICW4_8086		0x01				/* 8086/88 (instead of MCS-80/85) mode */
#define ICW4_AUTO		0x02				/* Auto (instead of normal) EOI */
#define ICW4_BUF_SLAVE	0x08				/* Buffered mode/slave */
#define ICW4_BUF_MASTER	0x0C				/* Buffered mode/master */
#define ICW4_SFNM		0x10				/* Special fully nested */

/* maximum number of a exception in a row */
#define MAX_EX_COUNT	3

/* the maximum length of messages (for interrupt-listeners) */
#define MSG_MAX_LEN		8

/* the address of the return-from-signal "function" in the startup.s */
#define SIGRETFUNC_ADDR	0xd

/* represents an IDT-entry */
typedef struct {
	/* The address[0..15] of the ISR */
	u16 offsetLow;
	/* Code selector that the ISR will use */
	u16 selector;
	/* these bits are fix: 0.1110.0000.0000b */
	u16 fix		: 13,
	/* the privilege level, 00 = ring0, 01 = ring1, 10 = ring2, 11 = ring3 */
	dpl			: 2,
	/* If Present is not set to 1, an exception will occur */
	present		: 1;
	/* The address[16..31] of the ISR */
	u16	offsetHigh;
} __attribute__((packed)) sIDTEntry;

/* represents an IDT-pointer */
typedef struct {
	u16 size;
	u32 address;
} __attribute__((packed)) sIDTPtr;

/* storage for "delayed" signal handling */
typedef struct {
	u8 active;
	tSig sig;
	fSigHandler handler;
} sSignalData;

/* isr prototype */
typedef void (*fISR)(void);

/**
 * Assembler routine to load an IDT
 */
extern void intrpt_loadidt(sIDTPtr *idt);

/**
 * Our ISRs
 */
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);
extern void isr32(void);
extern void isr33(void);
extern void isr34(void);
extern void isr35(void);
extern void isr36(void);
extern void isr37(void);
extern void isr38(void);
extern void isr39(void);
extern void isr40(void);
extern void isr41(void);
extern void isr42(void);
extern void isr43(void);
extern void isr44(void);
extern void isr45(void);
extern void isr46(void);
extern void isr47(void);
extern void isr48(void);
/* the handler for a other interrupts */
extern void isrNull(void);

/**
 * Inits the programmable interrupt controller
 */
static void intrpt_initPic(void);

/**
 * Sets the IDT-entry for the given interrupt
 *
 * @param number the interrupt-number
 * @param handler the ISR
 * @param dpl the privilege-level
 */
static void intrpt_setIDT(u16 number,fISR handler,u8 dpl);

/**
 * Sends EOI to the PIC, if necessary
 *
 * @param intrptNo the interrupt-number
 */
static void intrpt_eoi(u32 intrptNo);

/**
 * Checks for signals and notifies the corresponding process, if necessary
 */
static void intrpt_handleSignal(void);

/**
 * Finishes the signal-handling (does the user-stack-manipulation and so on)
 *
 * @param stack the interrupt-stack
 */
static void intrpt_handleSignalFinish(sIntrptStackFrame *stack);

/* interrupt -> name */
static cstring intrptNo2Name[] = {
	/* 0x00 */	"Divide by zero",
	/* 0x01 */	"Single step",
	/* 0x02 */	"Non maskable",
	/* 0x03 */	"Breakpoint",
	/* 0x04 */	"Overflow",
	/* 0x05 */	"Bounds check",
	/* 0x06 */	"Invalid opcode",
	/* 0x07 */	"Co-processor not available",
	/* 0x08 */	"Double fault",
	/* 0x09 */	"Co-processor segment overrun",
	/* 0x0A */	"Invalid TSS",
	/* 0x0B */	"Segment not present",
	/* 0x0C */	"Stack exception",
	/* 0x0D */	"General protection fault",
	/* 0x0E */	"Page fault",
	/* 0x0F */	"<unknown>",
	/* 0x10 */	"Co-processor error",
	/* 0x11 */	"<unknown>",
	/* 0x12 */	"<unknown>",
	/* 0x13 */	"<unknown>",
	/* 0x14 */	"<unknown>",
	/* 0x15 */	"<unknown>",
	/* 0x16 */	"<unknown>",
	/* 0x17 */	"<unknown>",
	/* 0x18 */	"<unknown>",
	/* 0x19 */	"<unknown>",
	/* 0x1A */	"<unknown>",
	/* 0x1B */	"<unknown>",
	/* 0x1C */	"<unknown>",
	/* 0x1D */	"<unknown>",
	/* 0x1E */	"<unknown>",
	/* 0x1F */	"<unknown>",
	/* 0x20 */	"Timer",
	/* 0x21 */	"Keyboard",
	/* 0x22 */	"<Cascade>",
	/* 0x23 */	"COM2",
	/* 0x24 */	"COM1",
	/* 0x25 */	"<unknown>",
	/* 0x26 */	"Floppy",
	/* 0x27 */	"<unknown>",
	/* 0x28 */	"CMOS real-time-clock",
	/* 0x29 */	"<unknown>",
	/* 0x2A */	"<unknown>",
	/* 0x2B */	"<unknown>",
	/* 0x2C */	"<unknown>",
	/* 0x2D */	"<unknown>",
	/* 0x2E */	"ATA1",
	/* 0x2F */	"ATA2",
	/* 0x30 */	"Syscall"
};

/* stuff to count exceptions */
static u32 exCount = 0;
static u32 lastEx = 0xFFFFFFFF;

/* pointer to the current interrupt-stack */
static sIntrptStackFrame *curIntrptStack = NULL;

/* the signal for a irq. SIG_COUNT = invalid */
static tSig irq2Signal[] = {
	SIG_INTRPT_TIMER,
	SIG_INTRPT_KB,
	SIG_COUNT,
	SIG_INTRPT_COM2,
	SIG_INTRPT_COM1,
	SIG_COUNT,
	SIG_INTRPT_FLOPPY,
	SIG_COUNT,
	SIG_INTRPT_CMOS,
	SIG_COUNT,
	SIG_COUNT,
	SIG_COUNT,
	SIG_COUNT,
	SIG_COUNT,
	SIG_INTRPT_ATA1,
	SIG_INTRPT_ATA2
};

/* temporary storage for signal-handling */
static sSignalData signalData;

/**
 * An assembler routine to load an IDT
 *
 * @param idt the IDT to load
 */
/*extern void idt_flush(sIDTPtr *idt);*/

/* the IDT */
static sIDTEntry idt[IDT_COUNT];

cstring intrpt_no2Name(u32 intrptNo) {
	if(intrptNo < ARRAY_SIZE(intrptNo2Name)) {
		return intrptNo2Name[intrptNo];
	}

	return "Unknown interrupt";
}

void intrpt_init(void) {
	u32 i;
	/* setup the idt-pointer */
	sIDTPtr idtPtr;
	idtPtr.address = (u32)idt;
	idtPtr.size = sizeof(idt) - 1;

	/* setup the idt */

	/* exceptions */
	intrpt_setIDT(0,isr0,IDT_DPL_KERNEL);
	intrpt_setIDT(1,isr1,IDT_DPL_KERNEL);
	intrpt_setIDT(2,isr2,IDT_DPL_KERNEL);
	intrpt_setIDT(3,isr3,IDT_DPL_KERNEL);
	intrpt_setIDT(4,isr4,IDT_DPL_KERNEL);
	intrpt_setIDT(5,isr5,IDT_DPL_KERNEL);
	intrpt_setIDT(6,isr6,IDT_DPL_KERNEL);
	intrpt_setIDT(7,isr7,IDT_DPL_KERNEL);
	intrpt_setIDT(8,isr8,IDT_DPL_KERNEL);
	intrpt_setIDT(9,isr9,IDT_DPL_KERNEL);
	intrpt_setIDT(10,isr10,IDT_DPL_KERNEL);
	intrpt_setIDT(11,isr11,IDT_DPL_KERNEL);
	intrpt_setIDT(12,isr12,IDT_DPL_KERNEL);
	intrpt_setIDT(13,isr13,IDT_DPL_KERNEL);
	intrpt_setIDT(14,isr14,IDT_DPL_KERNEL);
	intrpt_setIDT(15,isr15,IDT_DPL_KERNEL);
	intrpt_setIDT(16,isr16,IDT_DPL_KERNEL);
	intrpt_setIDT(17,isr17,IDT_DPL_KERNEL);
	intrpt_setIDT(18,isr18,IDT_DPL_KERNEL);
	intrpt_setIDT(19,isr19,IDT_DPL_KERNEL);
	intrpt_setIDT(20,isr20,IDT_DPL_KERNEL);
	intrpt_setIDT(21,isr21,IDT_DPL_KERNEL);
	intrpt_setIDT(22,isr22,IDT_DPL_KERNEL);
	intrpt_setIDT(23,isr23,IDT_DPL_KERNEL);
	intrpt_setIDT(24,isr24,IDT_DPL_KERNEL);
	intrpt_setIDT(25,isr25,IDT_DPL_KERNEL);
	intrpt_setIDT(26,isr26,IDT_DPL_KERNEL);
	intrpt_setIDT(27,isr27,IDT_DPL_KERNEL);
	intrpt_setIDT(28,isr28,IDT_DPL_KERNEL);
	intrpt_setIDT(29,isr29,IDT_DPL_KERNEL);
	intrpt_setIDT(30,isr30,IDT_DPL_KERNEL);
	intrpt_setIDT(31,isr31,IDT_DPL_KERNEL);
	intrpt_setIDT(32,isr32,IDT_DPL_KERNEL);

	/* hardware-interrupts */
	intrpt_setIDT(33,isr33,IDT_DPL_KERNEL);
	intrpt_setIDT(34,isr34,IDT_DPL_KERNEL);
	intrpt_setIDT(35,isr35,IDT_DPL_KERNEL);
	intrpt_setIDT(36,isr36,IDT_DPL_KERNEL);
	intrpt_setIDT(37,isr37,IDT_DPL_KERNEL);
	intrpt_setIDT(38,isr38,IDT_DPL_KERNEL);
	intrpt_setIDT(39,isr39,IDT_DPL_KERNEL);
	intrpt_setIDT(40,isr40,IDT_DPL_KERNEL);
	intrpt_setIDT(41,isr41,IDT_DPL_KERNEL);
	intrpt_setIDT(42,isr42,IDT_DPL_KERNEL);
	intrpt_setIDT(43,isr43,IDT_DPL_KERNEL);
	intrpt_setIDT(44,isr44,IDT_DPL_KERNEL);
	intrpt_setIDT(45,isr45,IDT_DPL_KERNEL);
	intrpt_setIDT(46,isr46,IDT_DPL_KERNEL);
	intrpt_setIDT(47,isr47,IDT_DPL_KERNEL);

	/* syscall */
	intrpt_setIDT(48,isr48,IDT_DPL_USER);

	/* all other interrupts */
	for(i = 49; i < 256; i++)
		intrpt_setIDT(i,isrNull,IDT_DPL_KERNEL);

	/* now we can use our idt */
	intrpt_loadidt(&idtPtr);

	/* now init the PIC */
	intrpt_initPic();
}

sIntrptStackFrame *intrpt_getCurStack(void) {
	return curIntrptStack;
}

static void intrpt_handleSignal(void) {
	tPid pid;
	tSig sig;
	/* already handling a signal? */
	if(signalData.active == 1)
		return;

	if(sig_hasSignal(&sig,&pid)) {
		fSigHandler handler = sig_startHandling(pid,sig);
		if(handler != NULL) {
			signalData.active = 1;
			signalData.sig = sig;
			signalData.handler = handler;
			/* a little trick: we store the signal to handle and manipulate the user-stack
			 * and so on later. if the process is currently running everything is fine. we return
			 * from here and intrpt_handleSignalFinish() will be called.
			 * if the target-process is not running we switch to him now. the process is somewhere
			 * in the kernel but he will leave the kernel IN EVERY CASE at the end of the interrupt-
			 * handler. So if we do the signal-stuff at the end we'll get there and will
			 * manipulate the user-stack.
			 * This is simpler than mapping the user-stack and kernel-stack of the other process
			 * in the current page-dir and so on :)
			 */
			if(proc_getRunning()->pid != pid) {
				/* ensure that the process is ready */
				sched_setReady(proc_getByPid(pid));
				proc_switchTo(pid);
			}
		}
	}
}

static void intrpt_handleSignalFinish(sIntrptStackFrame *stack) {
	u32 *esp = (u32*)stack->uesp;
	/* will handle copy-on-write */
	/* TODO we might have to add stack-pages... */
	paging_isRangeUserWritable((u32)(esp - 10),9 * sizeof(u32));

	/* save regs */
	*--esp = stack->eip;
	*--esp = stack->eax;
	*--esp = stack->ebx;
	*--esp = stack->ecx;
	*--esp = stack->edx;
	*--esp = stack->edi;
	*--esp = stack->esi;
	/* signal-number as argument */
	*--esp = signalData.sig;
	/* sigRet will remove the argument, restore the register,
	 * acknoledge the signal and return to eip */
	*--esp = SIGRETFUNC_ADDR;
	stack->eip = (u32)signalData.handler;
	stack->uesp = (u32)esp;

	/* we don't want to do this again */
	signalData.active = 0;
}

/* TODO temporary */
typedef struct {
	s8 name[MAX_PROC_NAME_LEN + 1];
	u8 *data;
} sProcData;
#include "../../build/services.txt"
static bool servicesLoaded = false;

static u64 umodeTime = 0;
static u64 kmodeTime = 0;
static u64 kmodeStart = 0;
static u64 kmodeEnd = 0;

void intrpt_handler(sIntrptStackFrame stack) {
	sProc *p;
	curIntrptStack = &stack;

	/*u32 syscallNo = stack.intrptNo == IRQ_SYSCALL ? ((sSysCallStack*)stack.uesp)->number : 0;*/

	kmodeStart = cpu_rdtsc();
	umodeTime += kmodeStart - kmodeEnd;

	/* add signal */
	switch(stack.intrptNo) {
		case IRQ_KEYBOARD:
		case IRQ_TIMER:
		case IRQ_ATA1:
		case IRQ_ATA2:
		case IRQ_CMOS_RTC:
		case IRQ_FLOPPY:
		case IRQ_COM1:
		case IRQ_COM2: {
			tSig sig = irq2Signal[stack.intrptNo - IRQ_MASTER_BASE];
			if(sig != SIG_COUNT)
				sig_addSignal(sig);
		}
		break;
	}

	if(servicesLoaded)
		vfsr_checkForMsgs();

	/*vid_printf("umodeTime=%d%%\n",(s32)(100. / (cpu_rdtsc() / (double)umodeTime)));*/
	switch(stack.intrptNo) {
		case IRQ_KEYBOARD:
			/* don't print info about intrpt */
			break;

		case IRQ_TIMER:
			/* TODO don't resched if we come from kernel-mode! */
			ASSERT(stack.ds == 0x23,"Timer interrupt from kernel-mode!");

			if(!servicesLoaded) {
				u32 i;
				servicesLoaded = true;
				for(i = 0; i < ARRAY_SIZE(services); i++) {
					/* clone proc */
					tPid pid = proc_getFreePid();
					if(proc_clone(pid)) {
						/* we'll reach this as soon as the scheduler has chosen the created process */
						p = proc_getRunning();
						/* remove data-pages */
						proc_changeSize(-p->dataPages,CHG_DATA);
						/* now load service */
						/* TODO just temporary */
						memcpy(p->command,services[i].name,strlen(services[i].name) + 1);
						elf_loadprog(services[i].data);
						proc_setupIntrptStack(&stack,0,NULL,0);
						/* we don't want to continue the loop ;) */
						break;
					}
				}
				break;
			}

			intrpt_eoi(stack.intrptNo);
			proc_switch();
			break;

		/* syscall */
		case IRQ_SYSCALL:
			sysc_handle(&stack);
			break;

		/* exceptions */
		case EX_DIVIDE_BY_ZERO ... EX_CO_PROC_ERROR:
			/* #PF */
			if(stack.intrptNo == EX_PAGE_FAULT) {
				/*vid_printf("Page fault for address=0x%08x @ 0x%x, process %d\n",cpu_getCR2(),
						stack.eip,proc_getRunning()->pid);*/
				if(!paging_handlePageFault(cpu_getCR2())) {
					panic("Page fault for address=0x%08x @ 0x%x",cpu_getCR2(),stack.eip);
				}
				break;
			}

			/* count consecutive occurrences */
			/* TODO we should consider irqs, too! */
			if(lastEx == stack.intrptNo) {
				exCount++;

				/* stop here? */
				if(exCount >= MAX_EX_COUNT)
					panic("Got this exception %d times. Stopping here\n",exCount);
			}
			else {
				exCount = 0;
				lastEx = stack.intrptNo;
			}

			/* #GPF */
			if(stack.intrptNo == EX_GEN_PROT_FAULT) {
				p = proc_getRunning();
				/* io-map not loaded yet? */
				if(p->ioMap != NULL && !tss_ioMapPresent()) {
					/* load it and give the process another try */
					tss_setIOMap(p->ioMap);
					exCount = 0;
					break;
				}

				vid_printf("GPF @ 0x%x\n",stack.eip);
				printStackTrace(getUserStackTrace(p,&stack));
				break;
			}
			/* fall through */

		default:
			vid_printf("Got interrupt %d (%s) @ 0x%x\n",stack.intrptNo,
					intrpt_no2Name(stack.intrptNo),stack.eip);
			break;
	}

	/* handle signals */
	intrpt_handleSignal();
	if(signalData.active == 1)
		intrpt_handleSignalFinish(&stack);

	/* send EOI to PIC */
	intrpt_eoi(stack.intrptNo);

	kmodeEnd = cpu_rdtsc();
	kmodeTime += kmodeEnd - kmodeStart;
	/*if((u32)(kmodeEnd - kmodeStart) > 2000000)
		vid_printf("SLOW(%d) %d %d\n",(u32)(kmodeEnd - kmodeStart),stack.intrptNo,syscallNo);
	else
		vid_printf("FAST(%d) %d %d\n",(u32)(kmodeEnd - kmodeStart),stack.intrptNo,syscallNo);*/
}

static void intrpt_initPic(void) {
	/* starts the initialization. we want to send a ICW4 */
	outb(PIC_MASTER_CMD,ICW1_INIT | ICW1_NEED_ICW4);
	outb(PIC_SLAVE_CMD,ICW1_INIT | ICW1_NEED_ICW4);
	/* remap the irqs to 0x20 and 0x28 */
	outb(PIC_MASTER_DATA,IRQ_MASTER_BASE);
	outb(PIC_SLAVE_DATA,IRQ_SLAVE_BASE);
	/* continue */
	outb(PIC_MASTER_DATA,4);
	outb(PIC_SLAVE_DATA,2);

	/* we want to use 8086 mode */
	outb(PIC_MASTER_DATA,ICW4_8086);
	outb(PIC_SLAVE_DATA,ICW4_8086);

	/* enable all interrupts (set masks to 0) */
	outb(PIC_MASTER_DATA,0x00);
	outb(PIC_SLAVE_DATA,0x00);
}

static void intrpt_setIDT(u16 number,fISR handler,u8 dpl) {
	idt[number].fix = 0xE00;
	idt[number].dpl = dpl;
	idt[number].present = number != IDT_INTEL_RES1 && number != IDT_INTEL_RES2;
	idt[number].selector = IDT_CODE_SEL;
	idt[number].offsetHigh = ((u32)handler >> 16) & 0xFFFF;
	idt[number].offsetLow = (u32)handler & 0xFFFF;
}

static void intrpt_eoi(u32 intrptNo) {
	/* do we have to send EOI? */
	if(intrptNo >= IRQ_MASTER_BASE && intrptNo <= IRQ_MASTER_BASE + IRQ_NUM) {
	    if(intrptNo >= IRQ_SLAVE_BASE) {
	    	/* notify the slave */
	        outb(PIC_SLAVE_CMD,PIC_EOI);
	    }

	    /* notify the master */
	    outb(PIC_MASTER_CMD,PIC_EOI);
    }
}

#if DEBUGGING

void intrpt_printStackFrame(sIntrptStackFrame *stack) {
	vid_printf("stack-frame @ 0x%x\n",stack);
	vid_printf("\tcs=%02x\n",stack->cs);
	vid_printf("\tds=%02x\n",stack->ds);
	vid_printf("\tes=%02x\n",stack->es);
	vid_printf("\tfs=%02x\n",stack->fs);
	vid_printf("\tgs=%02x\n",stack->gs);
	vid_printf("\teax=0x%08x\n",stack->eax);
	vid_printf("\tebx=0x%08x\n",stack->ebx);
	vid_printf("\tecx=0x%08x\n",stack->ecx);
	vid_printf("\tedx=0x%08x\n",stack->edx);
	vid_printf("\tesi=0x%08x\n",stack->esi);
	vid_printf("\tedi=0x%08x\n",stack->edi);
	vid_printf("\tebp=0x%08x\n",stack->ebp);
	vid_printf("\tuesp=0x%08x\n",stack->uesp);
	vid_printf("\teip=0x%08x\n",stack->eip);
	vid_printf("\teflags=0x%08x\n",stack->eflags);
	vid_printf("\terrorCode=%d\n",stack->errorCode);
	vid_printf("\tintrptNo=%d\n",stack->intrptNo);
}

#endif
