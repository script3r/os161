#ifndef __FILEDESCH__
#define __FILEDESCH__

#include <file.h>

#define MAX_OPEN_FILES 16
#define FD_RESERVED_SPOT 0xcafebabe

struct filedesc {
	struct file		*fd_ofiles[MAX_OPEN_FILES];	/* array of open files */
	struct lock		*fd_lk;				/* a lock protecting the file descriptor table */
	uint16_t		fd_nfiles;			/* how many open files we have */
};

void			fd_clone( struct filedesc *, struct filedesc * );
int			fd_create( struct filedesc ** );
void			fd_destroy( struct filedesc * );
int			fd_attach( struct filedesc *, struct file *, int * );
void			fd_detach( struct filedesc *, int );
int			fd_attach_into( struct filedesc *, struct file *, int );

#define FD_LOCK(x) (lock_acquire((x)->fd_lk))
#define FD_UNLOCK(x) (lock_release((x)->fd_lk))

#endif
