#include <types.h>
#include <lib.h>
#include <proc.h>
#include <file.h>
#include <filedesc.h>
#include <thread.h>
#include <current.h>
#include <syscall.h>

/**
 * exit will perform the following tasks:
 * 1. close all open files (through file_close_all())
 * 2. set the given exit code inside the proc structure.
 * 3. mark each child process as orphan.
 * 4. if orphan, will destroy the proc associated with the current thread.
 * 4.1 if not orphan, will signal the parent regarding our death.
 * 5. call thread_exit() so we become a zombie.
 */
void
sys__exit( int code ) {
	struct proc		*p = NULL;
	int			err;

	KASSERT( curthread != NULL );
	KASSERT( curthread->td_proc != NULL );
	
	p = curthread->td_proc;

	//close all open files.
	err = file_close_all( p );
	if( err ) 
		panic( "problem closing a file." );

	//lock so we can adjust the return value.
	PROC_LOCK( p );
	p->p_retval = code;
	p->p_is_dead = true;

	//if we are orphans ourselves, no one is interested
	//in our return code, so we simply destroy ourselves.
	if( p->p_proc == NULL ) {
		PROC_UNLOCK( p );
		proc_destroy( p );
	}
	else {
		//signal that we are done.
		V( p->p_sem );
		PROC_UNLOCK( p );
	}
	
	//all that is left now is to kill our thread.
	thread_exit();
}
