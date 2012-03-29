#include <types.h>
#include <lib.h>
#include <copyinout.h>
#include <proc.h>
#include <thread.h>
#include <current.h>
#include <filedesc.h>
#include <addrspace.h>
#include <kern/fcntl.h>
#include <limits.h>
#include <kern/errno.h>
#include <machine/trapframe.h>
#include <synch.h>
#include <vfs.h>
#include <syscall.h>

#ifndef NARG_MAX
#define NARG_MAX 1024
#endif

#define MAX_PROG_NAME 128

/**
 * given a string, and an align parameter
 * this function will align its length (by appending zero) to match the required alignment.
 */
static
int
align_arg( char arg[ARG_MAX], int align ) {
	char 	*p = arg;
	int	len = 0;
	int	diff;

	while( *p++ != '\0' )
		++len;
	
	//for our purpose, '\0' counts as a character also
	++len;
	if( len % align  == 0 )
		return len;

	diff = align - ( len % align );
	while( diff-- ) {
		*(++p) = '\0';
		++len;
	}

	return len;
}

static
int
copy_args( userptr_t uargs ) {
	int		err;
	int		i = 0;
	char		karg[ARG_MAX];
	char 		**kargs = (char **)uargs;
	size_t		kbuf_len = 0;

	//while there are parameters left.
	while( kargs[i] != NULL ) {
		//copyin the argument.
		err = copyinstr( (userptr_t)kargs[i], karg, sizeof( karg ), NULL );
		if( err )
			return err;

		kbuf_len += align_arg( karg, 4 );
		++i;
	}
	
	kbuf_len += ( i + 1 ) * sizeof( char * );
	return 0;
}

int	
sys_execv( userptr_t upname, userptr_t uargs ) {
	struct addrspace		*as_new = NULL;
	struct addrspace		*as_old = NULL;
	struct vnode			*vn = NULL;
	vaddr_t				entry_ptr;
	vaddr_t				stack_ptr;
	int				err;
	char				kpname[MAX_PROG_NAME];
	
	KASSERT( curthread != NULL );
	KASSERT( curthread->td_proc != NULL );
	
	//copy the old addrspace just in case.
	as_old = curthread->t_addrspace;

	//copyin the program name.
	err = copyinstr( upname, kpname, sizeof( kpname ), NULL );
	if( err )
		return err;
	
	//copyin the user arguments.
	err = copy_args( uargs );
	if( err )
		return err;

	//try to open the given executable.
	err = vfs_open( kpname, O_RDONLY, 0, &vn );
	if( err )
		return err;

	//create the new addrspace.
	as_new = as_create();
	if( as_new == NULL ) {
		vfs_close( vn );
		return ENOMEM;
	}
	
	
	//activate the new addrspace.
	as_activate( as_new );

	//temporarily switch the addrspaces.
	curthread->t_addrspace = as_new;

	//load the elf executable.
	err = load_elf( vn, &entry_ptr );
	if( err ) {
		curthread->t_addrspace = as_old;
		as_destroy( as_new );
		vfs_close( vn );
		return err;
	}

	//create a tack for the new addrspace.
	err = as_define_stack( as_new, &stack_ptr );
	if( err ) {
		curthread->t_addrspace = as_old;
		as_destroy( as_new );
		vfs_close( vn );
		return err;
	}

	//no need for it anymore.
	vfs_close( vn );

	//we are good to go.
	as_destroy( as_old );
	
	//off we go to userland.
	enter_new_process( 0, NULL, stack_ptr, entry_ptr );
	
	panic( "execv: we should not be here." );
	return EINVAL;
}
