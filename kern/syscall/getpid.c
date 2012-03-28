#include <types.h>
#include <lib.h>
#include <proc.h>
#include <thread.h>
#include <current.h>
#include <syscall.h>

int
sys_getpid( int *retval ) {
	KASSERT( curthread != NULL );
	KASSERT( curthread->td_proc != NULL );
	
	PROC_LOCK( curthread->td_proc );
	*retval = curthread->td_proc->p_pid;
	PROC_UNLOCK( curthread->td_proc );

	return 0;
}

