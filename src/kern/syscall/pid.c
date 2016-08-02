#include <types.h>
#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <syscall.h>
#include <copyinout.h>
#include <addrspace.h>
#include <mips/trapframe.h>
#include <spinlock.h>
#include <proc.h>
#include <spl.h>
#include <pid.h>
#include <kern/wait.h>

static struct process *pidtable[PID_MAX];

int process_init(int pid, struct proc *proc){
	struct process *p;

	p = kmalloc(sizeof(struct process));
	if (p == NULL) {
		return ENOMEM;
	}
	p->ppid = curproc->pid;
	p->pid_lock = lock_create("pid_lock");
	p->status = WUNTRACED;
	p->exitcode = 0;
	p->proc = proc;

	pidtable[pid] = p;

	lock_acquire(p->pid_lock);

	return 0;
}

int process_destroy(pid_t pid){
	kfree(pidtable[pid]);
	pidtable[pid] = NULL;

	return 0;
}

int pid_alloc(struct proc *proc){
	int result;

	for(int i = PID_MIN; i < PID_MAX; i++){
		if (pidtable[i] == NULL) {
			result = process_init(i, proc);
			if (result) {
				return result;
			}
			proc->pid = (pid_t) i;
			break;
		}
	}

	return 0;
}

/* PID related system calls */

int sys__exit(int exitcode){
	pid_t pid = curproc->pid;
	struct process *process;

	process = pidtable[pid];
	process->status = WNOHANG;
	process->exitcode = _MKWAIT_EXIT(exitcode);
	lock_release(process->pid_lock);

	

	return 0;
}

int sys_fork(struct trapframe *ptf, struct proc *pproc, pid_t *pid){
	struct proc *cproc;
	struct trapframe *ctf;
	struct addrspace *cas;
	int result;

	KASSERT(pproc == curproc);

	// cproc = kmalloc(sizeof(struct proc));
	cproc = proc_create_runprogram("child");
	if (cproc == NULL){
		return ENOMEM;
	}

	KASSERT(curproc->p_addrspace != NULL);

	result = as_copy(curproc->p_addrspace, &cas);
	if (result) {
		proc_destroy(cproc);
		return result;
	}

	KASSERT(cas != NULL);

	/* Allocate PID */
	*pid = cproc->pid;

	/* Make a copy of trapframe, so that two processes would not 
	 * have race condition.
	 */
	ctf = kmalloc(sizeof(struct trapframe));
	bzero(ctf, sizeof(struct trapframe));
	memcpy(ctf, (const void *)ptf, sizeof(struct trapframe));

	ctf->tf_a0 = (uint32_t)cas;

	result = thread_fork(curthread->t_name,
				cproc,
				(void *)enter_forked_process,
				(void *)ctf, 0);
	if (result) {
		proc_destroy(cproc);
		return result;
	}

	return 0;
}

int sys_getpid(pid_t *retval){
	*retval = curproc->pid;
	return 0;
}
