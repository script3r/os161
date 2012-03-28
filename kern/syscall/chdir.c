#include <types.h>
#include <lib.h>
#include <copyinout.h>
#include <kern/iovec.h>
#include <kern/fcntl.h>
#include <uio.h>
#include <vfs.h>
#include <thread.h>
#include <current.h>
#include <syscall.h>

#define MAX_DIR_LEN 128

int	
sys_chdir( userptr_t ubuf ) {
	char		kbuf[MAX_DIR_LEN];
	int		err;
	struct vnode	*v_dir = NULL;

	KASSERT( curthread != NULL );
	KASSERT( curthread->td_proc != NULL );
	
	//copy the buffer from userland into kernel land.
	err = copyinstr( ubuf, kbuf, sizeof( kbuf ), NULL );
	if( err )
		return err;

	//try to open the directory pointed by the path.
	err = vfs_open( kbuf, O_RDONLY, 0644, &v_dir );
	if( err )
		return err;

	//change the current directory.
	err = vfs_setcurdir( v_dir );
	
	//close the vnode.
	vfs_close( v_dir );

	if( err )
		return err;
	
	return 0;
}
