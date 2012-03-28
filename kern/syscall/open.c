#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <syscall.h>
#include <file.h>
#include <filedesc.h>
#include <copyinout.h>
#include <kern/fcntl.h>
#include <stat.h>
#include <vfs.h>
#include <current.h>

//check to see if the given flags are valid.
//valid flags consist of exactly one of O_RDONLY/O_WRONLY/O_RDWR.
static
bool
valid_flags( int flags ) {
	int		count = 0;
	int		accmode = flags & O_ACCMODE;

	if( accmode == O_RDWR )
		++count;
	
	if( accmode == O_RDONLY )
		++count;
	
	if( accmode == O_WRONLY )
		++count;

	return count == 1;
}

int 
sys_open( userptr_t upath, int flags, int *retval ) {
	struct file		*f;
	struct vnode		*vn;
	char			k_filename[MAX_FILE_NAME];
	int			err;
	struct proc 		*p;
	struct stat		st;

	//make sure the current thread is not null
	//and that we have a process associated with it
	KASSERT( curthread != NULL );
	KASSERT( curthread->td_proc != NULL );

	p = curthread->td_proc;

	//check if we have valid flags
	if( !valid_flags( flags ) )
		return EINVAL;

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

	//if we have O_APPEND, we must set the offset
	//to be the file size.
	if( flags & O_APPEND ) {
		err = VOP_STAT( f->f_vnode, &st );
		if( err ) {
			vfs_close( vn );
			file_destroy( f );
			return err;
		}
		
		f->f_offset = st.st_size;
	}

	//lock the file so we can safely modify
	F_LOCK( f );

	//we now need to find a spot inside the process' filetable
	//so we can store our new file
	err = fd_attach( p->p_fd, f, retval );
	if( err ) {
		F_UNLOCK( f );
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
