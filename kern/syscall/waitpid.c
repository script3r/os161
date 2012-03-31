#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <kern/wait.h>
#include <proc.h>
#include <copyinout.h>
#include <file.h>
#include <filedesc.h>
#include <thread.h>
#include <current.h>
#include <syscall.h>

int
___waitpid( int pid, int *retval, int options ) {
	struct proc		*p = NULL;
	int			err;

	KASSERT( curthread != NULL );
	KASSERT( curthread->td_proc != NULL );
	
	//we only support WNOHANG and nothing else.
	if( options != 0 && options != WNOHANG )
		return EINVAL;
	
	//get the process associated with the given pid
	err = proc_get( pid, &p );
	if( err )
		return err;

	//make sure that we are the parent of that process
	//otherwise we are collecting an the exit code of a process
	//whose parent might still be interested in.
	if( p->p_proc != curthread->td_proc ) {
		PROC_UNLOCK( p );
		return ECHILD;
	}
	
	//if WNOHANG was given, and said process is not yet dead
	//we immediately, (successfully) return with a value of 0.
	if( !p->p_is_dead && (options == WNOHANG) ) {
		PROC_UNLOCK( p );
		*retval = 0;
		return 0;
	}
	
	//unlock the process, so the child potentially makes progress.
	//then, consume a signal from the child's semaphore.
	PROC_UNLOCK( p );
	P( p->p_sem );
	PROC_LOCK( p );

	//at this point the child should be certainly dead.
	KASSERT( p->p_is_dead );
	
	//copy its exit code to the userland.
	*retval = p->p_retval;

	//unlock and destroy.
	PROC_UNLOCK( p );
	proc_destroy( p );
	
	return 0;

}

int
sys_waitpid( int pid, userptr_t uret, int options, int *retval ) {
	struct proc		*p = NULL;
	int			err;
	int			kstatus;

	KASSERT( curthread != NULL );
	KASSERT( curthread->td_proc != NULL );
	
	//we only support WNOHANG and nothing else.
	if( options != 0 && options != WNOHANG )
		return EINVAL;
	
	//get the process associated with the given pid
	err = proc_get( pid, &p );
	if( err )
		return err;

	//make sure that we are the parent of that process
	//otherwise we are collecting an the exit code of a process
	//whose parent might still be interested in.
	if( p->p_proc != curthread->td_proc ) {
		PROC_UNLOCK( p );
		return ECHILD;
	}
	
	//if WNOHANG was given, and said process is not yet dead
	//we immediately, (successfully) return with a value of 0.
	if( !p->p_is_dead && (options == WNOHANG) ) {
		PROC_UNLOCK( p );
		*retval = 0;
		return 0;
	}
	
	//unlock the process, so the child potentially makes progress.
	//then, consume a signal from the child's semaphore.
	PROC_UNLOCK( p );
	P( p->p_sem );
	PROC_LOCK( p );

	//at this point the child should be certainly dead.
	KASSERT( p->p_is_dead );
	
	//copy its exit code to the userland.
	kstatus = p->p_retval;
	err = copyout( &kstatus, uret, sizeof( int ) );

	//if we had an error copying out, we will return
	//an efault, but we still must destroy the associated
	//proc since the child is essentially dead.
	if( err ) {
		//unlock & destroy
		PROC_UNLOCK( p );
		proc_destroy( p );

		return err;
	}
	
	//the return value of waitpid must be the
	//pid that was passed in.
	*retval = pid;
	
	//unlock and destroy.
	PROC_UNLOCK( p );
	proc_destroy( p );
	
	return 0;
}
