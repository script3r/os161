#include <types.h>
#include <lib.h>
#include <vfs.h>
#include <kern/iovec.h>
#include <uio.h>
#include <kern/fcntl.h>
#include <kern/errno.h>
#include <vnode.h>
#include <current.h>
#include <syscall.h>

int	
sys___getcwd( userptr_t ubuf, size_t ulen,  int *retval ) {
	struct uio		ruio;
	struct iovec		iov;
	int 			err;

	KASSERT( curthread != NULL );
	KASSERT( curthread->td_proc != NULL );

	//prepare the uio
	uio_kinit(
		&iov,
		&ruio,
		ubuf,
		ulen,
		0,
		UIO_READ
	);

	//the given pointer lives in userland
	ruio.uio_space = curthread->t_addrspace;
	ruio.uio_segflg = UIO_USERSPACE;

	//forward the call to vfs_getcwd
	err = vfs_getcwd( &ruio );
	if( err )
		return err;

	//set the return value to be the length of the 
	//data that was just read
	*retval = ulen - ruio.uio_resid;
	return 0;
}

