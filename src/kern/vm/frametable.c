#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <synch.h>

/* Place your frametable data-structures here 
 * You probably also want to write a frametable initialisation
 * function and call it from vm_bootstrap
 */


static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

static struct frame_table_entry *frame_table;
static unsigned table_size;


/* Note that this function returns a VIRTUAL address, not a physical 
 * address
 * WARNING: this function gets called very early, before
 * vm_bootstrap().  You may wish to modify main.c to call your
 * frame table initialisation function, or check to see if the
 * frame table has been initialised and call ram_stealmem() otherwise.
 */

void frame_table_init(void)
{
	int n_used_page;
	uint32_t used_memory;
	paddr_t top;
	paddr_t bottom;

	top = ram_getsize();
	bottom = ram_getfirstfree();
	table_size = top / PAGE_SIZE;

	/*
	 * Put frame table at the top of used kernel memory.
	 * The size of frame table is set as the quotient of memory and PAGE_SIZE.
	 * So if the size of memory is not multiple of PAGE_SIZE,
	 * the residual will not be used.
	 */
	frame_table = (struct frame_table_entry *)PADDR_TO_KVADDR(bottom);

	used_memory = bottom + table_size * sizeof(struct frame_table_entry);

	n_used_page = used_memory / PAGE_SIZE +
					(used_memory % PAGE_SIZE ? 1 : 0);

	/*
	 * Used pages are set as FIXED, because they are directly mapped.
	 */
	for(int i = 0; i < n_used_page; i++){
		frame_table[i].state = FIXED;
	}

	for(unsigned i = n_used_page; i < table_size; i++){
		frame_table[i].state = FREE;
	}

}

vaddr_t
alloc_kpages(unsigned int npages)
{
	/*
	 * IMPLEMENT ME.  You should replace this code with a proper implementation.
	 */

	paddr_t addr = 0;

	if (npages > 1){
	}
	else{
		spinlock_acquire(&stealmem_lock);
		for(unsigned i = 0; i < table_size; i++){
			if (frame_table[i].state == FREE){
				frame_table[i].state = DIRTY;
				addr = PAGE_SIZE * i;
				break;
			}
		}
		spinlock_release(&stealmem_lock);
	}

	/*
	 * When no page can be allocated, return NULL.
	 */
	if(addr == 0){
		return (vaddr_t)NULL;
	}

	return PADDR_TO_KVADDR(addr);
}

void
free_kpages(vaddr_t addr)
{
	int index;

	index = (addr - MIPS_KSEG0) / PAGE_SIZE;
	spinlock_acquire(&stealmem_lock);
		frame_table[index].state = FREE;
	spinlock_release(&stealmem_lock);
}

