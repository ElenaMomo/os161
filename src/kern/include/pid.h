#ifndef _PID_H_
#define _PID_H_

#include <synch.h>

struct process {
    pid_t ppid;
    struct lock *pid_lock;
    bool status;
    int exitcode;
    struct proc *proc;
};

int process_init(int pid, struct proc *pproc);

int process_destroy(pid_t pid);

int pid_alloc(struct proc *);

#endif /* _PID_H_ */
