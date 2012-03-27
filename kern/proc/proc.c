#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <proc.h>

struct proc 		*allproc[MAX_PROCESSES];
struct lock 		*lk_allproc;

int
proc_create( struct proc **res ) {
	struct proc	*p = NULL;
	int		err = 0;

	p = kmalloc( sizeof( struct proc ) );
	if( p == NULL )
		return ENOMEM;
		
	//create the filedescriptor table
	err = fd_create( &p->p_fd );
	if( err ) {
		kfree( p );
		return err;
	}

	//create the lock
	p->p_lk = lock_create( "p_lk" );
	if( p->p_lk == NULL ) {
		fd_destroy( p->p_fd );
		kfree( p );
		return ENOMEM;
	}

	//create the cv
	p->p_cv = cv_create( "p_cv" );
	if( p->p_cv == NULL ) {
		lock_destroy( p->p_lk );
		fd_destroy( p->p_fd );
		kfree( p );
		return ENOMEM;
	}

	//adjust static information
	p->p_retval = 0;
	p->p_is_dead = false;
	p->p_nsyscalls = 0;
	p->p_nice = 0;
	
	*res = p;
	return 0;
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
}
