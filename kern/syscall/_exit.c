#include <types.h>
#include <lib.h>
#include <proc.h>
#include <file.h>
#include <filedesc.h>
#include <thread.h>
#include <current.h>
#include <syscall.h>

/**
 * loop over allproc, find children associated with the given process
 * and mark them as orphans.
 */
static
void
make_children_orphan( struct proc *p ) {
	int		i;

	lock_acquire( lk_allproc );
	for( i = 0; i < MAX_PROCESSES; ++i ) {
		//we are certainly not our own parent.
		if( allproc[i] == p )
			continue;
		
		//if it is a potential process, lock it, and if it is one of our children
		//mark it as orphan
		if( allproc[i] != NULL && allproc[i] != (void *) PROC_RESERVED_SPOT ) {
			PROC_LOCK( allproc[i] );
			if( allproc[i]->p_proc == p ) 
				allproc[i]->p_proc = NULL;
			PROC_UNLOCK( allproc[i] );
		}
	}
	lock_release( lk_allproc );
}
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
	PROC_UNLOCK( p );

	//orphan our children.
	make_children_orphan( p );
	
	//lock for atomicity
	PROC_LOCK( p );

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
