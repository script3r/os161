#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <file.h>
#include <vfs.h>

/*
 * create a new file associated with the given vnode/flags
 */
int
file_create( struct vnode *vn, int flags, struct file **f ) {
	struct file *res;

	res = kmalloc( sizeof( struct file ) );
	if( res == NULL )
		return ENOMEM;

	//fill the basic fields
	res->f_oflags = flags;
	res->f_refcount = 0;
	res->f_vnode = vn;
	res->f_offset = 0;

	//attempt to create the lock
	res->f_lk = lock_create( "f_lk" );
	if( res->f_lk == NULL ) {
		kfree( res );
		return ENOMEM;
	}

	*f = res;
	return 0;
}

/*
 * destroy the given file.
 * otherwise, lock_destroy will fail.
 */
void
file_destroy( struct file *f ) {
	//make sure we are not destroying something that is being used
	KASSERT( f->f_refcount == 0 );
	
	//close the associated vnode
	vfs_close( f->f_vnode );
	
	//release and destroy the lock
	lock_destroy( f->f_lk );

	//free the memory
	kfree( f );
}

/**
 * close the file given by the descriptor
 */
int
file_close_descriptor( struct proc *p, int fd ) {
	struct file 	*f = NULL;
	int 		err = 0;

	err = file_get( p, fd, &f );
	if( err )
		return err;
	
	//make sure there are programs using it
	KASSERT( f->f_refcount > 0 );

	//detach from the file descriptor table
	fd_detach( p->p_fd, fd );
	
	//decrease both refcounts
	f->f_refcount--;
	VOP_DECREF( f->f_vnode );

	//destroy if we are the only ones using it
	if( f->f_refcount == 0 ) {
		F_UNLOCK( f );
		file_destroy( f );
		return 0;
	}

	//unlock the file
	F_UNLOCK( f );
	return 0;
}


/**
 * find and return the file associated with the filedescriptor
 * inside the process. it will be returned locked.
 */
int		
file_get(struct proc *p, int fd, struct file **f ) {
	if( fd >= MAX_OPEN_FILES || fd < 0 )
		return EBADF;

	FD_LOCK( p->p_fd );
	if( p->p_fd->fd_ofiles[fd] != NULL ) { 
		*f = p->p_fd->fd_ofiles[fd];
		F_LOCK( *f );
		FD_UNLOCK( p->p_fd );
		return 0;
	}
	
	FD_UNLOCK( p->p_fd );
	return EBADF;
}

/**
 * Checks whether the given file descriptor exists in the table.
 */
bool	
file_descriptor_exists( struct proc *p, int fd ) {
	bool 		exists = false;

	FD_LOCK( p->p_fd );
	if( p->p_fd->fd_ofiles[fd] != NULL )
		exists = true;
	FD_UNLOCK( p->p_fd );
	
	return exists;
}

/**
 * close all open files associated with the given process.
 */
int
file_close_all( struct proc *p ) {
	int		i = 0;
	int		err;

	FD_LOCK( p->p_fd );
	for( i = 0; i < MAX_OPEN_FILES; ++i ) {
		if( p->p_fd->fd_ofiles[i] != NULL ) {
			FD_UNLOCK( p->p_fd );
			err = file_close_descriptor( p, i );
			if( err )
				return -1;
			FD_LOCK( p->p_fd );
		}
	}
	FD_UNLOCK( p->p_fd );
	return 0;
}
