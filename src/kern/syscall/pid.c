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

int sys_getpid(int *retval){
	*retval = (int)curthread->t_proc->pid;
	return 0;
}

int sys_fork(struct trapframe *ptf, struct proc *pproc, pid_t *pid){
	struct proc *cproc;
	struct trapframe *ctf;
	struct addrspace *cas;
	int result;

	cproc = kmalloc(sizeof(struct proc));
	if (cproc == NULL){
		return ENOMEM;
	}
	spinlock_init(&cproc->p_lock);

	result = as_copy(curproc->p_addrspace, &cas);
	if (result) {
		kfree(curproc);
		return result;
	}

	/* Copy CWD */
	cproc->p_cwd = pproc->p_cwd;
	VOP_INCREF(cproc->p_cwd);

	/* Allocate PID */
	cproc->pid = 1;
	*pid = cproc->pid;

	/* Copy trapframe for child proc*/
	ctf = kmalloc(sizeof(struct trapframe));
	bzero(ctf, sizeof(struct trapframe));
	memcpy(ctf, (const void *)ptf, sizeof(struct trapframe));

	ctf->tf_a0 = (uint32_t)pproc->p_addrspace;
	ctf->tf_a1 = (uint32_t)curproc;
	ctf->tf_a2 = (uint32_t)cas;

	result = thread_fork(curthread->t_name,
				cproc,
				(void *)enter_forked_process,
				(void *)ctf,1);
	if (result) {
		proc_destroy(cproc);
		return result;
	}

	return 0;
}
