#include <types.h>
#include <lib.h>
#include <copyinout.h>
#include <kern/errno.h>
#include <file.h>
#include <filedesc.h>
#include <kern/iovec.h>
#include <kern/fcntl.h>
#include <uio.h>
#include <proc.h>
#include <current.h>
#include <syscall.h>

int	
sys_write( int fd, userptr_t ubuf, size_t ulen,  int *retval ) {
	int		err 		= 0;
	struct file	*f 		= NULL;
	struct proc	*p 		= NULL;
	char		kbuf[ulen];
	struct uio	wuio;
	struct iovec	iov;

	KASSERT( curthread != NULL );
	KASSERT( curthread->td_proc != NULL );

	p = curthread->td_proc;

	//attempt to get a hold of the file
	err = file_get( p, fd, &f );
	if( err )
		return err;

	//make sure it is not readony
	if( f->f_oflags & O_RDONLY )
		return EBADF;
	
	//copy the data from userland into kernel
	err = copyin( ubuf, kbuf, sizeof( kbuf ) );
	if( err ) {
		F_UNLOCK( f );
		return err;
	}

	//prepare the iovec/uio
	uio_kinit(
		&iov,
		&wuio,
		kbuf,
		sizeof( kbuf ),
		f->f_offset,
		UIO_WRITE
	);

	//perform the IO
	err = VOP_WRITE( f->f_vnode, &wuio );
	if( err ) {
		F_UNLOCK( f );
		return err;
	}

	//update the offset
	f->f_offset = wuio.uio_offset;
	F_UNLOCK( f );

	//number of bytes written
	*retval = ulen - wuio.uio_resid;
	return 0;
}		
