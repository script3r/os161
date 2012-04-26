#include <types.h>
#include <lib.h>
#include <copyinout.h>
#include <proc.h>
#include <thread.h>
#include <current.h>
#include <filedesc.h>
#include <addrspace.h>
#include <kern/errno.h>
#include <machine/trapframe.h>
#include <synch.h>
#include <syscall.h>

/**
 * structure containings all the information
 * necessary for the child fork thread.
 */
struct child_fork_args {
	struct addrspace	*as_source;
	struct proc		*td_proc;
	struct trapframe	*tf;
};
	

/**
 * this is the first function that gets executed
 * when the child thread runs.
 */
static
void
fork_child_return( void *v_args, unsigned long not_used ) {
	struct child_fork_args 	*args = NULL;
	struct trapframe	tf;
	
	(void)not_used;

	//cast to something we can work with.
	args = v_args;
	
	//the return value of fork() for the child is 0.
	args->tf->tf_v0 = 0;
	args->tf->tf_a3 = 0;

	//skip to the next instruction to avoid fork()ing again.
	args->tf->tf_epc += 4;

	//make the current thread aware of its process.
	curthread->td_proc = args->td_proc;
	
	//set the current addrspace.
	KASSERT( curthread->t_addrspace == NULL );
	curthread->t_addrspace = args->as_source;
	
	//activate it.
	as_activate( curthread->t_addrspace );

	//copy from kernel stack into user stack.
	memcpy( &tf, args->tf, sizeof( struct trapframe ) );

	//clean-up the arguments passed by fork().
	kfree( args->tf );
	kfree( args );
	
	//off we go to usermode.
	mips_usermode( &tf );
}

/**
 * helper function to clone a trapframe.
 */
static
int
trapframe_clone( struct trapframe *tf, struct trapframe **newtf ) {
	*newtf = kmalloc( sizeof( struct trapframe ) );
	if( *newtf == NULL )
		return ENOMEM;
	
	memcpy( *newtf, tf, sizeof( struct trapframe ) );
	return 0;
}

int
sys_fork( struct trapframe *tf, int *retval ) {
	struct proc		*p_new = NULL;
	struct trapframe	*tf_new = NULL;
	int			err;
	struct child_fork_args	*args;
	pid_t			pid;

	KASSERT( curthread != NULL );
	KASSERT( curthread->td_proc != NULL );
	KASSERT( tf != NULL );

	//attempt to create a clone.
	err = proc_clone( curthread->td_proc, &p_new );
	if( err )
		return err;
	
	//hold on to the pid.
	pid = p_new->p_pid;

	//set the parent process of the new process to be us.
	p_new->p_proc = curthread->td_proc;
	
	//clone the trapframe.
	err = trapframe_clone( tf, &tf_new );
	if( err ) {	
		file_close_all( p_new );
		proc_destroy( p_new );
		return err;
	}

	//prepare the arguments for the fork child thread.
	args = kmalloc( sizeof( struct child_fork_args ) );
	if( args == NULL ) {
		kfree( tf_new );
		file_close_all( p_new );
		proc_destroy( p_new );
		return ENOMEM;
	}
	
	//copy the trapframe and wrapping proc
	//into the args structure.
	args->tf = tf_new;
	args->td_proc = p_new;

	//copy the addresspace.
	err = as_copy( curthread->t_addrspace, &args->as_source );
	if( err ) {
		//clean after ourselves.
		kfree( args->tf );
		kfree( args );
		
		file_close_all( p_new );
		proc_destroy( p_new );
		return err;
	}
	
	//finalize the creation of the thread.
	err = thread_fork(
		curthread->t_name,
		fork_child_return,
		args,
		0,
		NULL
	);
	
	//oh well, thread creation failed.
	//make sure we clean-up after ourselves.
	if( err ) {
		as_destroy( args->as_source );
		kfree( args->tf );
		kfree( args );

		//close all possible files open by the proc.
		file_close_all( p_new );
		proc_destroy( p_new );
		return err;
	}
	

	//parent returns with no errors.
	*retval = pid;
	return 0;	
}
