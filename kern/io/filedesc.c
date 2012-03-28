#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <filedesc.h>

//scan over the given file descriptor table
//and store the file in a free location, if possible
int
fd_attach( struct filedesc *fdesc, struct file *f, int *fd ) {
	int 		i = 0;

	//lock the file descriptor table
	FD_LOCK( fdesc );

	//for each possible spot
	for( i = 0; i < MAX_OPEN_FILES; ++i ) {
		if( fdesc->fd_ofiles[i] == NULL ) {
			fdesc->fd_ofiles[i] = f;
			fdesc->fd_nfiles++;
			*fd = i;
			FD_UNLOCK( fdesc );
			return 0;
		}
	}
	FD_UNLOCK( fdesc );
	return ENFILE;
}

//detaches the given filedescriptor from the given filetable
void
fd_detach( struct filedesc *fdesc, int fd ) {
	FD_LOCK( fdesc );
	fdesc->fd_ofiles[fd] = NULL;
	fdesc->fd_nfiles--;
	FD_UNLOCK( fdesc );
}

//destroy the given filedescriptor table
void
fd_destroy( struct filedesc *fdesc ) {
	KASSERT( fdesc->fd_nfiles == 0 );

	lock_destroy( fdesc->fd_lk );
	kfree( fdesc );
}

//create a new filedescriptor tabke
int
fd_create( struct filedesc **fdesc ) {
	struct filedesc 	*fd = NULL;
	int 			i = 0;

	fd = kmalloc( sizeof( struct filedesc ) );
	if( fd == NULL ) 
		return ENOMEM;

	//create the lock
	fd->fd_lk = lock_create( "fd_lk" );
	if( fd->fd_lk == NULL ) {
		kfree( fd );
		return ENOMEM;
	}

	//initialize all open files
	for( i = 0; i < MAX_OPEN_FILES; ++i ) 
		fd->fd_ofiles[i] = NULL;

	//initially we have no open files.
	fd->fd_nfiles = 0;

	//we are good to go
	*fdesc = fd;
	return 0;
}

/**
 * attaches a given filehandle inside a specific location
 * on the filetable.
 */
int
fd_attach_into( struct filedesc *fdesc, struct file *f, int fd ) {
	FD_LOCK( fdesc );

	//if the file-table already contains something
	//inside the desired spot, we cannot continue.
	if( fdesc->fd_ofiles[fd] != NULL ) {
		FD_UNLOCK( fdesc );
		return EMFILE;
	}

	fdesc->fd_ofiles[fd] = f;
	fdesc->fd_nfiles++;

	FD_UNLOCK( fdesc );
	return 0;
}
