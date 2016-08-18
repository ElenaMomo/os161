#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>
#include <current.h>
#include <proc.h>
#include <spl.h>

/* Place your page table functions here */


void vm_bootstrap(void)
{
    /* Initialise VM sub-system.  You probably want to initialise your 
       frame table here as well.
    */
    frame_table_init();
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
    struct addrspace *as;
    int pt1, pt2;
    uint32_t ehi, elo;
    paddr_t addr;
    int spl;

    // panic("vm_fault hasn't been written yet\n");

    switch (faulttype) {
        case VM_FAULT_READONLY:
            panic("dumbvm: got VM_FAULT_READONLY\n");
        case VM_FAULT_READ:
        case VM_FAULT_WRITE:
            break;
        default:
            return EINVAL;
    }

    if (faultaddress >= USERSTACK) {
        return EFAULT;
    }

    if (curproc == NULL) {
        /*
         * No process. This is probably a kernel fault early
         * in boot. Return EFAULT so as to panic instead of
         * getting into an infinite faulting loop.
         */
        return EFAULT;
    }

    as = proc_getas();
    if (as == NULL) {
        /*
         * No address space set up. This is probably also a
         * kernel fault early in boot.
         */
        return EFAULT;
    }

    /* Assert that the address space has been set up properly. */
    KASSERT(as->as_vbase1 != 0);
    KASSERT(as->as_npages1 != 0);
    KASSERT(as->as_vbase2 != 0);
    KASSERT(as->as_npages2 != 0);
    KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);

    pt1 = PT1_INDEX(faultaddress);
    pt2 = PT2_INDEX(faultaddress);

    if (as->page_table[pt1] == NULL){
        as->page_table[pt1] = kmalloc(sizeof(page_table_entry) * PAGE_TABLE_SIZE);
    }

    if (as->page_table[pt1][pt2] == 0) {
        getppages(as, faultaddress, 1);
        return 0;
    }

    addr = as->page_table[pt1][pt2] - MIPS_KSEG0;

    KASSERT((addr & PAGE_FRAME) == addr);

    spl = splhigh();

    // ehi = faultaddress;
    // elo = addr | TLBLO_DIRTY | TLBLO_VALID;
    // tlb_random(ehi, elo);

    for (int i=0; i<NUM_TLB; i++) {
        tlb_read(&ehi, &elo, i);
        if (elo & TLBLO_VALID) {
            continue;
        }
        ehi = faultaddress & PAGE_FRAME;
        elo = addr | TLBLO_DIRTY | TLBLO_VALID;
        DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, addr);
        tlb_write(ehi, elo, i);
        splx(spl);
        return 0;
    }

    splx(spl);

    return 0;
}

/*
 *
 * SMP-specific functions.  Unused in our configuration.
 */

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("vm tried to do tlb shootdown?!\n");
}

void
vm_tlbshootdown_all(void)
{
    // panic("vm tried to do tlb shootdown?!\n");
    for(int i = 0; i < NUM_TLB; i++){
        tlb_write(TLBHI_INVALID(0),TLBLO_INVALID(), i);
    }
}
