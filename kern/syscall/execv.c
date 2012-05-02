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

static char		karg[ARG_MAX];
static unsigned char	kargbuf[ARG_MAX];

#define MAX_PROG_NAME 32

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
	
	if( ++len % align  == 0 )
		return len;

	diff = align - ( len % align );
	while( diff-- ) {
		*(++p) = '\0';
		++len;
	}

	return len;
}

/**
 * return the nearest length aligned to alignment.
 */
static
int
get_aligned_length( char arg[ARG_MAX], int alignment ) {
	char *p = arg;
	int len = 0;

	while( *p++ != '\0' )
		++len;

	if( ++len % 4 == 0 )
		return len;
	
	return len + (alignment - ( len % alignment ) );
}

static
int
copy_args( userptr_t uargs, int *nargs, int *buflen ) {
	int		i = 0;
	int		err;
	int		nlast = 0;
	char		*ptr;
	unsigned char	*p_begin = NULL;
	unsigned char	*p_end = NULL;
	uint32_t	offset;
	uint32_t	last_offset;

	//check whether we got a valid pointer.
	if( uargs == NULL )
		return EFAULT;

	//initialize the numbe of arguments and the buffer size
	*nargs = 0;
	*buflen = 0;

	//copy-in kargs.
	i = 0;
	while( ( err = copyin( (userptr_t)uargs + i * 4, &ptr, sizeof( ptr ) ) ) == 0 ) {
		if( ptr == NULL )
			break;
		err = copyinstr( (userptr_t)ptr, karg, sizeof( karg ), NULL );
		if( err ) 
			return err;
		
		++i;
		*nargs += 1;
		*buflen += get_aligned_length( karg, 4 ) + sizeof( char * );
	}

	//if there is a problem, and we haven't read a single argument
	//that means the given user argument pointer is invalid.
	if( i == 0 && err )
		return err;
	
	//account for NULL also.
	*nargs += 1;
	*buflen += sizeof( char * );
	
	
	//loop over the arguments again, building karbuf.
	i = 0;
	p_begin = kargbuf;
	p_end = kargbuf + (*nargs * sizeof( char * ));
	nlast = 0;
	last_offset = *nargs * sizeof( char * );
	while( ( err = copyin( (userptr_t)uargs + i * 4, &ptr, sizeof( ptr ) ) ) == 0 ) {
		if( ptr == NULL )
			break;
		err = copyinstr( (userptr_t)ptr, karg, sizeof( karg ), NULL );
		if( err ) 
			return err;
		
		offset = last_offset + nlast;
		nlast = align_arg( karg, 4 );

		//copy the integer into 4 bytes.
		*p_begin = offset & 0xff;
		*(p_begin + 1) = (offset >> 8) & 0xff;
		*(p_begin + 2) = (offset >> 16) & 0xff;
		*(p_begin + 3) = (offset >> 24) & 0xff;
		
		//copy the string the buffer.
		memcpy( p_end, karg, nlast );
		p_end += nlast;

		//advance p_begin by 4 bytes.
		p_begin += 4;

		//adjust last offset
		last_offset = offset;
		++i;
	}
	
	//set the NULL pointer (i.e., it takes 4 zero bytes.)
	*p_begin = 0;
	*(p_begin+1) = 0;
	*(p_begin+2) = 0;
	*(p_begin+3) = 0;
	
	return 0;
}

static
int
adjust_kargbuf( int nparams, vaddr_t stack_ptr ) {
	int 		i;
	uint32_t	new_offset = 0;
	uint32_t	old_offset = 0;
	int		index;

	for( i = 0; i < nparams-1; ++i ) {
		index = i * sizeof( char * );
		//read the old offset.
		old_offset = (( 0xFF & kargbuf[index+3] ) << 24) |  (( 0xFF & kargbuf[index+2]) << 16) |
			     (( 0xFF & kargbuf[index+1]) << 8) |   (0xFF & kargbuf[index]);
		
		//calculate the new offset
		new_offset = stack_ptr + old_offset;
		
		//store it instead of the old one.
		memcpy( kargbuf + index, &new_offset, sizeof( int ) );
	}

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
	int				nargs;
	int				buflen;

	KASSERT( curthread != NULL );
	KASSERT( curthread->td_proc != NULL );
	
	(void)uargs;
	
	//lock the execv args
	lock_acquire( lk_exec );

	//copy the old addrspace just in case.
	as_old = curthread->t_addrspace;

	//copyin the program name.
	err = copyinstr( upname, kpname, sizeof( kpname ), NULL );
	if( err ) {
		lock_release( lk_exec );
		return err;
	}
	
	//try to open the given executable.
	err = vfs_open( kpname, O_RDONLY, 0, &vn );
	if( err ) {
		lock_release( lk_exec );
		return err;
	}

	//copy the arguments into the kernel buffer.
	err = copy_args( uargs, &nargs, &buflen );
	if( err ) {
		lock_release( lk_exec );
		vfs_close( vn );
		return err;
	}

	//create the new addrspace.
	as_new = as_create();
	if( as_new == NULL ) {
		lock_release( lk_exec );
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
		as_activate( as_old );
	
		as_destroy( as_new );
		vfs_close( vn );
		lock_release( lk_exec );
		return err;
	}

	//create a stack for the new addrspace.
	err = as_define_stack( as_new, &stack_ptr );
	if( err ) {
		curthread->t_addrspace = as_old;
		as_activate( as_old );

		as_destroy( as_new );
		vfs_close( vn );
		lock_release( lk_exec );
		return err;
	}
	
	//adjust the stackptr to reflect the change
	stack_ptr -= buflen;
	err = adjust_kargbuf( nargs, stack_ptr );
	if( err ) {
		curthread->t_addrspace = as_old;
		as_activate( as_old );

		as_destroy( as_new );
		vfs_close( vn );
		lock_release( lk_exec );
		return err;
	}

	//copy the arguments into the new user stack.
	err = copyout( kargbuf, (userptr_t)stack_ptr, buflen );
	if( err ) {
		curthread->t_addrspace = as_old;
		as_activate( as_old );
		as_destroy( as_new );
		vfs_close( vn );
		lock_release( lk_exec );
		return err;
	}

	//reelase lk_exec
	lock_release( lk_exec );

	//no need for it anymore.
	vfs_close( vn );

	//we are good to go.
	as_destroy( as_old );
	
	//off we go to userland.
	enter_new_process( nargs-1, (userptr_t)stack_ptr, stack_ptr, entry_ptr );
	
	panic( "execv: we should not be here." );
	return EINVAL;
}
