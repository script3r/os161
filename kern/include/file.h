#ifndef __FILEH__
#define __FILEH__

#include <vnode.h>
#include <synch.h>
#include <proc.h>

#define MAX_FILE_NAME 32

struct proc;

struct file {
	struct vnode			*f_vnode;	/* vnode associated with the file */
	uint16_t			f_oflags;	/* open flags */
	uint16_t			f_refcount;	/* reference count */
	off_t				f_offset;	/* file offset */
	struct lock			*f_lk;		/* lock for IO atomicity */
};

int		file_get(struct proc *, int, struct file ** );
int		file_close_descriptor( struct proc *, int );
int		file_close( struct proc *, struct file * );
int		file_create( struct vnode *, int, struct file ** );
bool		file_descriptor_exists( struct proc *, int );
void		file_destroy( struct file * );
int		file_close_all( struct proc * );


//helper function to open() files from inside the kernel.
int		___open( struct proc *, char *, int, int *);

#define F_LOCK(x) (lock_acquire((x)->f_lk))
#define F_UNLOCK(x) (lock_release((x)->f_lk))

#endif
