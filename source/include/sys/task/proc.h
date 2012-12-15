/**
 * $Id$
 * Copyright (C) 2008 - 2011 Nils Asmussen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef SYS_PROC_H_
#define SYS_PROC_H_

#include <sys/common.h>
#include <sys/mem/region.h>
#include <sys/mem/paging.h>
#include <sys/mem/vmreg.h>
#include <sys/mem/vmfree.h>
#include <sys/task/elf.h>
#include <sys/intrpt.h>

#ifdef __i386__
#include <sys/arch/i586/task/proc.h>
#endif
#ifdef __eco32__
#include <sys/arch/eco32/task/proc.h>
#endif
#ifdef __mmix__
#include <sys/arch/mmix/task/proc.h>
#endif

/* max number of coexistent processes */
#define MAX_PROC_COUNT		8192
#define MAX_FD_COUNT		64

/* for marking unused */
#define INVALID_PID			(MAX_PROC_COUNT + 1)
#define KERNEL_PID			MAX_PROC_COUNT

#define ROOT_UID			0
#define ROOT_GID			0

/* process flags */
#define P_ZOMBIE			1
#define P_PREZOMBIE			2
#define P_BOOT				4

#define PLOCK_COUNT			5
#define PLOCK_ENV			0
#define PLOCK_REGIONS		1
#define PLOCK_FDS			2
#define PLOCK_PORTS			3
#define PLOCK_PROG			4	/* clone, exec, threads */

struct sThread;

typedef struct {
	klock_t lock;
	int refCount;
	size_t count;
	gid_t *groups;
} sProcGroups;

typedef struct {
	pid_t pid;
	/* the signal that killed the process (SIG_COUNT if none) */
	int signal;
	/* exit-code the process gave us via exit() */
	int exitCode;
	/* memory it has used */
	ulong ownFrames;
	ulong sharedFrames;
	ulong swapped;
	/* other stats */
	uint64_t runtime;
	ulong schedCount;
	ulong syscalls;
} sExitState;

/* represents a process */
typedef struct {
	/* flags for vm86 and zombie */
	uint8_t flags;
	/* process id (2^16 processes should be enough :)) */
	const pid_t pid;
	/* parent process id */
	pid_t parentPid;
	/* real, effective and saved user-id */
	uid_t ruid;
	uid_t euid;
	uid_t suid;
	/* real, effective and saved group-id */
	gid_t rgid;
	gid_t egid;
	gid_t sgid;
	/* all groups (may include egid or not) of this process */
	sProcGroups *groups;
	/* the physical address for the page-directory of this process */
	pagedir_t pagedir;
	/* the number of frames the process owns, i.e. no cow, no shared stuff, no mapphys.
	 * paging-structures are counted, too */
	ulong ownFrames;
	/* the number of frames the process uses, but maybe other processes as well */
	ulong sharedFrames;
	/* pages that are in swap */
	ulong swapped;
	/* for finding free positions more quickly: the start for the stacks */
	uintptr_t freeStackAddr;
	/* address of the text- and data-region */
	uintptr_t textAddr;
	uintptr_t dataAddr;
	/* area-map for the free area */
	sVMFreeMap freemap;
	/* the regions */
	sVMRegTree regtree;
	/* the entrypoint of the binary */
	uintptr_t entryPoint;
	/* file descriptors: point into the global file table */
	sFile *fileDescs[MAX_FD_COUNT];
	/* channels to send/receive messages to/from fs (needed in vfs/real.c) */
	sSLList fsChans;
	/* environment-variables of this process */
	sSLList *env;
	/* for the waiting parent */
	sExitState *exitState;
	/* the address of the sigRet "function" */
	uintptr_t sigRetAddr;
	/* architecture-specific attributes */
	sProcArchAttr archAttr;
	/* start-command */
	const char *command;
	/* threads of this process */
	sSLList threads;
	/* the directory-node-number in the VFS of this process */
	inode_t threadDir;
	/* locks for this process */
	klock_t locks[PLOCK_COUNT];
	struct {
		/* I/O stats */
		ulong input;
		ulong output;
	} stats;
} sProc;

/* the area for proc_chgsize() */
typedef enum {CHG_DATA,CHG_STACK} eChgArea;

/**
 * Inits the page-directory
 */
void proc_preinit(void);

/**
 * Initializes the process-management
 */
void proc_init(void);

/**
 * @return the page-dir of the current process (or the first one)
 */
pagedir_t *proc_getPageDir(void);

/**
 * Sets the command of <p> to <cmd> and frees the current command-string, if necessary
 *
 * @param p the process
 * @param cmd the new command-name
 */
void proc_setCommand(sProc *p,const char *cmd);

/**
 * @return the pid of the running process
 */
pid_t proc_getRunning(void);

/**
 * Returns the process with given id WITHOUT aquiring a lock.
 *
 * @param pid the pid of the process
 * @return the process with given pid
 */
sProc *proc_getByPid(pid_t pid);

/**
 * Requests the process with given id and given lock.
 *
 * @param pid the process-id
 * @param l the lock: PLOCK_*
 * @return the process
 */
sProc *proc_request(pid_t pid,size_t l);

/**
 * Releases the lock of given process.
 *
 * @param p the process
 * @param l the lock: PLOCK_*
 */
void proc_release(sProc *p,size_t l);

/**
 * @return the number of existing processes
 */
size_t proc_getCount(void);

/**
 * Determines the memory-usage for the given process
 *
 * @param pid the process-id
 * @param own will be set to the number of own frames
 * @param shared will be set to the number of shared frames
 * @param swapped will be set to the number of swapped frames
 */
void proc_getMemUsageOf(pid_t pid,size_t *own,size_t *shared,size_t *swapped);

/**
 * @param p the process
 * @return the number of frames that is used by the given process in the kernel-space
 */
size_t proc_getKMemUsageOf(sProc *p);

/**
 * Determines the mem-usage of all processes
 *
 * @param dataShared will point to the number of shared bytes
 * @param dataOwn will point to the number of own bytes
 * @param dataReal will point to the number of really used bytes (considers sharing)
 */
void proc_getMemUsage(size_t *dataShared,size_t *dataOwn,size_t *dataReal);

/**
 * Adds the given signal to the given process. If necessary, the process is killed.
 *
 * @param pid the process-id
 * @param signal the signal
 */
void proc_addSignalFor(pid_t pid,int signal);

/**
 * Clones the current process, gives the new process a clone of the current thread and saves this
 * thread in proc_clone() so that it will start there on thread_resume().
 *
 * @param flags the flags to set for the process (e.g. P_VM86)
 * @return < 0 if an error occurred, the child-pid for parent, 0 for child
 */
int proc_clone(uint8_t flags);

/**
 * Initializes the architecture specific parts of the given process
 *
 * @param dst the clone
 * @param src the parent
 * @return 0 on success
 */
int proc_cloneArch(sProc *dst,const sProc *src);

/**
 * Starts a new thread at given entry-point. Will clone the kernel-stack from the current thread
 *
 * @param entryPoint the address where to start
 * @param flags the thread-flags
 * @param arg the argument
 * @return < 0 if an error occurred or the new tid
 */
int proc_startThread(uintptr_t entryPoint,uint8_t flags,const void *arg);

/**
 * Executes the given program, i.e. removes all regions except the stacks and loads the new one.
 *
 * @param path the path to the program
 * @param args the arguments
 * @param code if the program is already in memory, the location of it (used for boot-modules)
 * @param size if code != NULL, the size of the program in memory
 * @return 0 on success
 */
int proc_exec(const char *path,const char *const *args,const void *code,size_t size);

/**
 * Removes all regions from the given process
 *
 * @param pid the process-id
 * @param remStack whether the stack should be removed too
 */
void proc_removeRegions(pid_t pid,bool remStack);

/**
 * Waits until the thread with given thread-id or all other threads of the process are terminated.
 *
 * @param tid the thread to wait for (0 = all other threads)
 */
void proc_join(tid_t tid);

/**
 * Destroys the current thread. If it's the only thread in the process, the complete process will
 * destroyed.
 * Note that the actual deletion will be done later!
 *
 * @param exitCode the exit-code
 */
void proc_exit(int exitCode);

/**
 * Sends a SIG_SEGFAULT signal to the current process and performs a thread-switch if the process
 * has been terminated.
 */
void proc_segFault(void);

/**
 * Marks the given process as zombie and notifies the waiting parent thread. As soon as the parent
 * thread fetches the exit-state we'll kill the process
 *
 * @param pid the process-id
 * @param exitCode the exit-code to store
 * @param signal the signal with which it was killed (SIG_COUNT if none)
 */
void proc_terminate(pid_t pid,int exitCode,int signal);

/**
 * Kills the given thread. If the process has no threads afterwards, it is destroyed.
 * This is ONLY called by the terminator-module!
 *
 * @param tid the thread-id
 */
void proc_killThread(tid_t tid);

/**
 * Destroys the process with given pid, i.e. all threads are killed immediatly, the exit-state
 * is stored and all process-resources are free'd. After that, the parent can grab the exit-state.
 * Note: This function is only used from the unit-tests. By default, the terminator uses
 * proc_killThread to kill threads, which will destroy the process if no thread is left.
 *
 * @param pid the process-id
 */
void proc_destroy(pid_t pid);

/**
 * Waits until a child-process terminated and copies its exit-state to <state>.
 *
 * @param state the state to write to (if not NULL)
 * @return 0 on success
 */
int proc_waitChild(USER sExitState *state);

/**
 * Handles the architecture-specific part of the terminate-operation.
 *
 * @param p the process
 */
void proc_terminateArch(sProc *p);

/**
 * Kills the given process. This is done after the exit-state has been given to the parent.
 *
 * @param pid the process-id
 */
void proc_kill(pid_t pid);

/**
 * Copies the arguments (for exec) in <args> to <*argBuffer> and takes care that everything
 * is ok. <*argBuffer> will be allocated on the heap, so that is has to be free'd when you're done.
 *
 * @param args the arguments to copy
 * @param argBuffer will point to the location where it has been copied (to be used by
 * 	proc_setupUserStack())
 * @param size will point to the size the arguments take in <argBuffer>
 * @param fromUser whether the arguments are from user-space
 * @return the number of arguments on success or < 0
 */
int proc_buildArgs(const char *const *args,char **argBuffer,size_t *size,bool fromUser);

/**
 * Prints all existing processes
 */
void proc_printAll(void);

/**
 * Prints the regions of all existing processes
 */
void proc_printAllRegions(void);

/**
 * Prints the given parts of the page-directory for all existing processes
 *
 * @param parts the parts (see paging_printPageDirOf)
 * @param regions whether to print the regions too
 */
void proc_printAllPDs(uint parts,bool regions);

/**
 * Prints the given process
 *
 * @param p the pointer to the process
 */
void proc_print(sProc *p);


#if DEBUGGING

/**
 * Starts profiling all processes
 */
void proc_dbg_startProf(void);

/**
 * Stops profiling all processes and prints the result
 */
void proc_dbg_stopProf(void);

#endif

#endif /*SYS_PROC_H_*/
