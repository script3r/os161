#include <types.h>
#include <lib.h>
#include <proc.h>
#include <file.h>
#include <vnode.h>
#include <vfs.h>
#include <stat.h>
#include <kern/fcntl.h>
#include <kern/seek.h>
#include <kern/errno.h>
#include <current.h>
#include <syscall.h>

int	
sys_lseek( int fd, off_t offset, int whence, int64_t *retval ) {
	struct proc		*p = NULL;
	struct file		*f = NULL;
	int			err;
	struct stat		st;

	KASSERT( curthread != NULL );
	KASSERT( curthread->td_proc != NULL );

	p = curthread->td_proc;

	//try to open the file
	err = file_get( p, fd, &f );
	if( err )
		return err;

	//use VOP_TRYSEEK to verify whether the desired
	//seeking location is proper.
	err = VOP_TRYSEEK( f->f_vnode, offset );
	if( err ) {
		F_UNLOCK( f );
		return err;
	}
	
	//depending on whence, seek to appropriate location
	switch( whence ) {
		case SEEK_SET:
			f->f_offset = offset;
			break;
		
		case SEEK_CUR:
			f->f_offset += offset;
			break;

		case SEEK_END:
			//if it is SEEK_END, we use VOP_STAT to figure out
			//the size of the file, and set the offset to be that size.
			err = VOP_STAT( f->f_vnode, &st );
			if( err ) {
				F_UNLOCK( f );
				return err;
			}

			//set the offet to the filesize.
			f->f_offset = st.st_size + offset;
			break;
		default:
			F_UNLOCK( f );
			return EINVAL;
	}
	
	*retval = f->f_offset;
	F_UNLOCK( f );
	return 0;
}	
