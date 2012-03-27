#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <syscall.h>
#include <file.h>
#include <copyinout.h>
#include <current.h>

/**
 * close() system call
 */
int
sys_close( int fd ) {
	struct proc 		*p = NULL;

	KASSERT( curthread != NULL );
	KASSERT( curthread->td_proc != NULL );
	
	p = curthread->td_proc;
	return file_close_descriptor( p, fd );
}
