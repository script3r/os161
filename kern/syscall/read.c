#include <types.h>
#include <lib.h>
#include <copyinout.h>
#include <proc.h>
#include <kern/iovec.h>
#include <uio.h>
#include <file.h>
#include <kern/fcntl.h>
#include <kern/errno.h>
#include <vnode.h>
#include <current.h>
#include <syscall.h>

int
sys_read( int fd, userptr_t ubuf, size_t ulen, int *retval ) {
	struct proc		*p = NULL;
	struct file		*f = NULL;
	struct uio		ruio;
	struct iovec		iov;
	int			err;

	KASSERT( curthread != NULL );
	KASSERT( curthread->td_proc != NULL );

	p = curthread->td_proc;
	
	//attempt to open the file
	err = file_get( p, fd, &f );
	if( err )
		return err;

	//check whether we have reading permissions
	if( f->f_oflags & O_WRONLY ) {
		F_UNLOCK( f );
		return EBADF;
	}

	//prepare the uio for read
	uio_kinit (
		&iov,
		&ruio,
		ubuf,
		ulen,
		f->f_offset,
		UIO_READ
	);

	ruio.uio_space = curthread->t_addrspace;
	ruio.uio_segflg = UIO_USERSPACE;

	//perform the read
	err = VOP_READ( f->f_vnode, &ruio );
	
	if( err ) {
		F_UNLOCK( f );
		return err;
	}

	//advance the offset
	f->f_offset = ruio.uio_offset;

	//set the number of bytes read to be retval
	*retval = ulen - ruio.uio_resid;

	//unlock and quit
	F_UNLOCK( f );
	return 0;
}
