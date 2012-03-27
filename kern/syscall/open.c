#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <syscall.h>
#include <file.h>
#include <filedesc.h>
#include <copyinout.h>
#include <vfs.h>
#include <current.h>

int 
sys_open( userptr_t upath, int flags, int *retval ) {
	struct file		*f;
	struct vnode		*vn;
	char			k_filename[MAX_FILE_NAME];
	int			err;
	struct proc 		*p;

	//make sure the current thread is not null
	//and that we have a process associated with it
	KASSERT( curthread != NULL );
	KASSERT( curthread->td_proc != NULL );

	p = curthread->td_proc;

	//copy the path from userland into kernel-land
	err = copyinstr( upath, k_filename, sizeof( k_filename ), NULL );
	
	//if we couldn't copy the file name, we probably have an I/O error
	//simply return that error code
	if( err )
		return err;
	
	//attempt to open the file
	err = vfs_open( k_filename, flags, 0, &vn );
	if( err )
		return err;
	
	//atempt to create the file object
	err = file_create( vn, flags, &f );
	if( err ) {
		vfs_close( vn );
		return err;
	}

	//lock the file so we can safely modify
	F_LOCK( f );

	//we now need to find a spot inside the process' filetable
	//so we can store our new file
	err = fd_attach( p->p_fd, f, retval );
	if( err ) {
		vfs_close( vn );
		file_destroy( f );
		return err;
	}

	//increase both references counts
	f->f_refcount++;
	VOP_INCREF( f->f_vnode );
	
	//we are done if the file, unlock it
	F_UNLOCK( f );
	return 0;
}
