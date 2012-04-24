#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/stat.h>
#include <lib.h>
#include <uio.h>
#include <bitmap.h>
#include <synch.h>
#include <thread.h>
#include <current.h>
#include <mainbus.h>
#include <addrspace.h>
#include <vm.h>
#include <vm/swap.h>
#include <machine/coremap.h>
#include <vfs.h>
#include <vnode.h>

void
swap_in( paddr_t target, off_t source ) {
	(void)target;
	(void)source;
}
