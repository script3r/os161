#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <proc.h>

struct proc 		*allproc[MAX_PROCESSES];
struct lock 		*lk_allproc;
int			next_pid;

/**
 * add the given function to the allproc array.
 */
static
void
proc_add_to_allproc( struct proc *p, int spot ) {
	lock_acquire( lk_allproc );

	KASSERT( allproc[spot] == (void *)PROC_RESERVED_SPOT );
	allproc[spot] = p;
	lock_release( lk_allproc );
}

/**
 * quick function that simply a spot as reserved,
 * stores the index of the spot inside the pid
 * and unlocks allproc.
 */
static
void
proc_found_spot( pid_t *pid, int spot ) {
	//if we are being called, allproc[spot] must be null.
	KASSERT( allproc[spot] == NULL );
	
	//mark it as reserved.
	allproc[spot] = (void *)PROC_RESERVED_SPOT;
	*pid = spot;
			
	//adjust next_pid to be the one just given.
	next_pid = spot + 1;

	//release the lock
	lock_release( lk_allproc );
}

/**
 * attempts to allocate a new pid by looping
 * through allproc, starting from next_pid, until finding an empty spot.
 * once a spot is given, it is marked as reserved.
 */
static
int
proc_alloc_pid( pid_t *pid ) {
	int		i = 0;

	//lock allproc, to guarantee atomicity
	lock_acquire( lk_allproc );
	
	//if the next_pid is greater than the maximum number of processes
	//we need to start failing pid allocations.
	if( next_pid >= MAX_PROCESSES )
		next_pid = 0;
	
	
	//first we loop until the end, starting from lastpid.
	for( i = next_pid; i < MAX_PROCESSES; ++i ) {
		if( allproc[i] == NULL ) {
			proc_found_spot( pid, i );
			return 0;
		}
	}

	//now we loop from the beginning until next_pid
	for( i = 0; i < next_pid; ++i ) {
		if( allproc[i] == NULL ) {
			proc_found_spot( pid, i );
			return 0;
		}
	}

	//we couldn't find a spot, so we have to fail.
	lock_release( lk_allproc );
	return ENPROC;
}

/**
 * de-allocates the given pid.
 */
static
void
proc_dealloc_pid( pid_t pid ) {
	//lock for atomicity.
	lock_acquire( lk_allproc );

	//it cannot be null, it must be either reserved or fulfilled.
	KASSERT( allproc[pid] != NULL );
	allproc[pid] = NULL;
	
	//unlock and be done.
	lock_release( lk_allproc );
}

int
proc_create( struct proc **res ) {
	struct proc	*p = NULL;
	int		err = 0;
	pid_t		pid;

	//first, attempt to allocate a pid.
	err = proc_alloc_pid( &pid );
	if( err )
		return err;

	//alloc memory for the structure
	p = kmalloc( sizeof( struct proc ) );
	if( p == NULL ) {
		proc_dealloc_pid( pid );
		return ENOMEM;
	}
		
	//associcate it with the pid.
	p->p_pid = pid;

	//create the filedescriptor table
	err = fd_create( &p->p_fd );
	if( err ) {
		kfree( p );
		proc_dealloc_pid( pid );
		return err;
	}

	//create the lock
	p->p_lk = lock_create( "p_lk" );
	if( p->p_lk == NULL ) {
		fd_destroy( p->p_fd );
		kfree( p );
		proc_dealloc_pid( pid );
		return ENOMEM;
	}

	//create the cv
	p->p_cv = cv_create( "p_cv" );
	if( p->p_cv == NULL ) {
		lock_destroy( p->p_lk );
		fd_destroy( p->p_fd );
		kfree( p );
		proc_dealloc_pid( pid );
		return ENOMEM;
	}

	//adjust static information
	p->p_retval = 0;
	p->p_is_dead = false;
	p->p_nsyscalls = 0;
	p->p_nice = 0;
	p->p_proc = NULL;

	//add to the list of allproc
	proc_add_to_allproc( p, pid );

	*res = p;
	return 0;
}

/**
 * destroy a given process.
 */
void
proc_destroy( struct proc *p ) {
	pid_t		pid;

	//copy the pid for later used.
	pid = p->p_pid;

	//destroy the cv
	cv_destroy( p->p_cv );

	//destroy the lock associated with it.
	lock_destroy( p->p_lk );

	//destroy the filedescriptor table
	fd_destroy( p->p_fd );

	//free the memory.
	kfree( p );

	//deallocate the pid.
	proc_dealloc_pid( pid );
}

/**
 * initialize the proc-system
 */
void 
proc_system_init( void ) {
	int 		i = 0;
	
	//initialize the array
	for( i = 0; i < MAX_PROCESSES; ++i ) {
		allproc[i] = NULL;
	}

	//create the lock
	lk_allproc = lock_create( "lk_allproc" );
	if( lk_allproc == NULL ) 
		panic( "could not initialize proc system." );

	//set last pid to be 0.
	next_pid = 0;
}

/** 
 * stress tests.
 */
void
proc_test_pid_allocation() {
	pid_t 		pid;
	int 		err;
	int 		i;

	for( i = 0; i < 100; ++i ) {
		err = proc_alloc_pid( &pid );
		if( err )
			panic( "failed to allocate a pid. pid allocation is broken.\n" );
		
		kprintf( "awarded PID = %d\n", pid );
		proc_dealloc_pid( pid );	
	}
}



