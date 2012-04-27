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

struct bitmap		*bm_sw;
struct lock		*lk_sw;
struct swap_stats	ss_sw;
struct vnode		*vn_sw;
struct lock 		*giant_paging_lock;

static
bool
swap_device_suficient( size_t ram_size ) {
	size_t		min_size;
	struct stat	stat;

	min_size = ram_size * SWAP_MIN_FACTOR;
	VOP_STAT( vn_sw, &stat );
	
	return stat.st_size >= min_size;
}

static
void
swap_init_stats( size_t ram_size ) {
	ss_sw.ss_total = ram_size / PAGE_SIZE;
	ss_sw.ss_free = ss_sw.ss_total;
	ss_sw.ss_reserved = 0;
	ss_sw.ss_used = 0;
}


static
void
swap_ensure_integrity() {
	KASSERT( ss_sw.ss_total == ss_sw.ss_free + ss_sw.ss_reserved + ss_sw.ss_used );
}

static
void
swap_io( paddr_t paddr, off_t offset, enum uio_rw op ) {
	struct iovec		iov;
	struct uio		uio;
	vaddr_t			vaddr;
	int			res;

	//get the virtual address.
	vaddr = PADDR_TO_KVADDR( paddr );

	//init the uio request.
	uio_kinit( &iov, &uio, (char *)vaddr, PAGE_SIZE, offset, op );
	
	//perform the request.
	res = (op == UIO_READ) ? VOP_READ( vn_sw, &uio ) : VOP_WRITE( vn_sw, &uio );
	
	//if we have a problem ... bail.
	if( res )
		panic( "swap_io: failed to perform a VOP." );
}

void
swap_bootstrap() {
	char		sdevice[64];
	int		res;
	size_t		ram_size;

	//get the ram size.
	ram_size = ROUNDUP( mainbus_ramsize(), PAGE_SIZE );

	//prepare to open the swap device.
	strcpy( sdevice, SWAP_DEVICE );

	//open.
	res = vfs_open( sdevice, O_RDWR, 0, &vn_sw );
	if( res )
		panic( "swap_bootstrap: could not open swapping partition." );
	
	//make sure it is of suficient size.
	if( !swap_device_suficient( ram_size ) )
		panic( "swap_bootstrap: the swap partition is not large enough." );
	
	//init the stats.
	swap_init_stats( ram_size );

	//create the bitmap to manage the swap partition.
	bm_sw = bitmap_create( ss_sw.ss_total );
	if( bm_sw == NULL ) 
		panic( "swap_bootstrap: could not create the swap bitmap." );

	lk_sw = lock_create( "lk_sw" );
	if( lk_sw == NULL )
		panic( "swap_bootstrap: could not create the swap lock." );
	//create the giant paging lock.
	giant_paging_lock = lock_create( "giant_paging_lock" );
	if( giant_paging_lock == NULL ) 
		panic( "vm_bootstrap: could not create giant_paging_lock." );

	//remove the first page.
	bitmap_mark( bm_sw, 0 );

	//update stats.
	--ss_sw.ss_free;
	++ss_sw.ss_used;
}

off_t		
swap_alloc() {
	unsigned	ix;
	int		res;

	LOCK_SWAP();
	res = bitmap_alloc( bm_sw, &ix );
	if( res )
		panic( "swap_alloc: swap partition is full!" );

	//update stats
	--ss_sw.ss_free;
	++ss_sw.ss_used;

	swap_ensure_integrity();
	UNLOCK_SWAP();

	return ix * PAGE_SIZE;
}

void
swap_dealloc( off_t offset ) {
	int		ix;
	
	//the index is simply the offset divided by page size.
	ix = offset / PAGE_SIZE;
	
	//lock the swap.
	LOCK_SWAP();
	
	//mark this index as unused.
	bitmap_unmark( bm_sw, ix );
	
	//update stats.
	++ss_sw.ss_free;
	--ss_sw.ss_used;
	
	//unlock the swap.
	UNLOCK_SWAP();
}

void
swap_in( paddr_t target, off_t source ) {
	swap_io( target, source, UIO_READ );
}

void
swap_out( paddr_t source, off_t target ) {
	swap_io( source, target, UIO_WRITE );
}
