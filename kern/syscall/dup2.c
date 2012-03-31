#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <filedesc.h>
#include <file.h>
#include <current.h>
#include <syscall.h>

int	
sys_dup2( int oldfd, int newfd, int *retval ) {
	struct proc		*p = NULL;
	struct file		*f_old = NULL;
	struct file		*f_new = NULL;
	int			err;

	KASSERT( curthread != NULL );
	KASSERT( curthread->td_proc != NULL );

	p = curthread->td_proc;
	
	//make sure both file handles are valid
	if( (oldfd < 0 || newfd < 0) || (newfd >= MAX_OPEN_FILES) )
		return EBADF;

	//get the old file
	err = file_get( p, oldfd, &f_old );
	if( err )
		return EBADF;

	//if the new-file already exists
	//we must close it.
	if( file_descriptor_exists( p, newfd ) ) {
		err = file_close_descriptor( p, newfd );
		//if we had a problem closing, dup2()
		//cannot continue running.
		if( err ) {
			F_UNLOCK( f_old );
			return err;
		}
	}

	//at this point, we simply copy the pointers.
	f_new = f_old;
	
	//attach f_new into newfd
	err = fd_attach_into( p->p_fd, f_new, newfd );
	if( err ) {
		F_UNLOCK( f_old );
		return err;
	}
	
	//increase the reference count.
	f_old->f_refcount++;
	VOP_INCREF(f_old->f_vnode);

	//unlock and return
	F_UNLOCK( f_old );
	*retval = newfd;

	return 0;
}	
