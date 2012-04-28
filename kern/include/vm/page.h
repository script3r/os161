#ifndef _VM_PAGE_H
#define _VM_PAGE_H

/**
 * this struct represents a logical page.
 * it is the basic object that the vm system manages.
 */
struct vm_page {
	volatile paddr_t		vmp_paddr;	/* the current physical address of this page */
	off_t				vmp_swapaddr;	/* offset into the swap partition */
	struct spinlock			vmp_slk;	/* spinlock protecting the members */
	bool				vmp_in_transit;	/* page is in transit */
};

#define VM_PAGE_IN_CORE(vmp) (((vmp)->vmp_paddr & PAGE_FRAME) != INVALID_PADDR)
#define VM_PAGE_IN_BACKING(vmp) ((vmp)->vmp_swapaddr != INVLALID_SWAPADDR)
#define VM_PAGE_IS_LOCKED(vmp) (KASSERT(spinlock_do_i_hold(&(vmp)->vmp_slk)))

#define VM_PAGE_DIRTY 0x01

struct vm_page 		*vm_page_create( void );
void			vm_page_destroy( struct vm_page * );
void			vm_page_lock( struct vm_page * );
void			vm_page_unlock( struct vm_page * );
void			vm_page_wire( struct vm_page * );
int			vm_page_clone( struct vm_page *, struct vm_page ** );
int			vm_page_new_blank( struct vm_page ** );
int			vm_page_fault( struct vm_page *, struct addrspace *, int fault_type, vaddr_t );
bool			vm_page_evict( struct vm_page * );

extern struct wchan	*wc_transit;

#endif
