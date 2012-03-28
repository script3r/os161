#ifndef __PROC__
#define __PROC__

#include <types.h>
#include <filedesc.h>
#include <synch.h>

#define MAX_PROCESSES 32
#define PROC_RESERVED_SPOT 0xcafebabe

struct proc {
	pid_t			p_pid;		/* pid of the process */
	struct filedesc		*p_fd;		/* file descriptor table */
	struct proc		*p_proc;	/* parent process */
	bool			p_is_dead;	/* are we dead? */
	int			p_retval;	/* our return code */

	/* synchronization mechanisms */
	struct lock		*p_lk;		/* lock to protect the structure */
	struct semaphore	*p_sem;		/* sem used for wait/exit */

	/* scheduler related */
	uint64_t		p_nsyscalls;	/* how many system calls we called? */
	int			p_nice;		/* our nice value */
};

extern struct proc *allproc[MAX_PROCESSES];
extern struct lock *lk_allproc;

int		proc_create( struct proc ** );
struct proc * 	proc_clone(struct proc *);
void 		proc_destroy(struct proc *);
struct proc * 	proc_get( pid_t );
void		proc_system_init(void);

//tests.
void		proc_test_pid_allocation(void);

#define PROC_LOCK(x) (lock_acquire( (x)->p_lk ))
#define PROC_UNLOCK(x) (lock_release( (x)->p_lk ))

#endif
