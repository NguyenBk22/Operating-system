// #ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct rg_elmt)
{
	struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

	if (rg_elmt.rg_start >= rg_elmt.rg_end)
		return -1;

	// if (rg_node != NULL)
	//   rg_elmt.rg_next = rg_node;
	// unnecessary if-condition
	rg_elmt.rg_next = rg_node; // work correctly even if rg_node is null

	/* Enlist the new region */
	mm->mmap->vm_freerg_list = &rg_elmt;

	return 0;
}

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
	struct vm_area_struct *pvma = mm->mmap;

	if (mm->mmap == NULL)
		return NULL;

	int vmait = 0;

	while (vmait < vmaid)
	{
		if (pvma == NULL)
			return NULL;

		pvma = pvma->vm_next;

		++vmait; // increment counter to prevent infinite loop
	}

	return pvma;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
	if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ) // rgid > PAGING_MAX_SYMTBL_SZ is wrong condition
		return NULL;

	return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
{
	/* Allocate at the toproof */
	struct vm_rg_struct rgnode;

	if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
	{
		caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
		caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;

		*alloc_addr = rgnode.rg_start;

		return 0;
	}

	/* TODO DONE: get_free_vmrg_area FAILED handle the region management (Fig.6) */

	/*Attempt to increase limit to get space */
	struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
	int inc_sz = PAGING_PAGE_ALIGNSZ(size);
	// int inc_limit_ret
	int old_sbrk = cur_vma->sbrk;

	/* TODO DONE: increase the limit
	 * inc_vma_limit(caller, vmaid, inc_sz)
	 */
	if (inc_vma_limit(caller, vmaid, inc_sz) == 0)
	{ /*Successful increase limit */
		caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
		caller->mm->symrgtbl[rgid].rg_end = old_sbrk + size;

		*alloc_addr = old_sbrk;

		return 0;
	}

	return 1;
}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
	if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
		return -1;

	/* TODO DONE: Manage the collect freed region to freerg_list */
	struct vm_rg_struct rgnode = *get_symrg_byid(caller->mm, rgid);

	/*enlist the obsoleted memory region */
	enlist_vm_freerg_list(caller->mm, rgnode);

	return 0;
}

/*pgalloc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int pgalloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
	int addr;

	/* By default using vmaid = 0 */
	return __alloc(proc, 0, reg_index, size, &addr);
}

/*pgfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int pgfree_data(struct pcb_t *proc, uint32_t reg_index)
{
	return __free(proc, 0, reg_index);
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
	uint32_t pte = mm->pgd[pgn];

	if (!PAGING_PAGE_PRESENT(pte))
	{ /* Page is not online, make it actively living */
		int vicpgn, swpfpn;
		// int vicfpn;
		// uint32_t vicpte;

		int tgtfpn = PAGING_SWP(pte); // the target frame storing our variable

		/* TODO DONE: Play with your paging theory here */

		/* Find victim page */
		if (find_victim_page(caller->mm, &vicpgn) < 0)
		{ /* No free page found in ram, can't perform swap */
			return -1;
		}

		/* Get free frame in MEMSWP */
		if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn) < 0)
		{ /* No free fram found in swp, can't perform swap */
			return -1;
		}

		/* Do swap frame from MEMRAM to MEMSWP and vice versa */
		uint32_t vicpte = mm->pgd[vicpgn];
		int vicfpn = PAGING_FPN(vicpte);

		/* Copy victim frame to swap */
		__swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);

		/* Copy target frame from swap to mem */
		__swap_cp_page(caller->active_mswp, tgtfpn, caller->mram, vicfpn);

		/* Update page table */

		/* Update the victim page table entry */
		/* UNFINISHED: where to get swap type and swap offset of swpfpn?
		 * The below implementation is just a guess for swap type and swap offset
		 */
		// pte_set_swap(mm->pgd + pgn, swptyp, swpoff) &mm->pgd;
		int vic_swaptyp = 0;
		int vic_swapoff = swpfpn;
		pte_set_swap(mm->pgd + vicpgn, vic_swaptyp, vic_swapoff);

		/* Update the target page table entry */
		// pte_set_fpn() &mm->pgd[pgn];
		pte_set_fpn(mm->pgd + pgn, vicfpn);

		/* The page is now present and ready to use now,
		 * so we'll push it onto the list of free page
		 */
		enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
	}

	pte = mm->pgd[pgn];
	*fpn = PAGING_FPN(pte);

	return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
	int pgn = PAGING_PGN(addr);
	int off = PAGING_OFFST(addr);
	int fpn;

	/* Get the page to MEMRAM, swap from MEMSWAP if needed */
	if (pg_getpage(mm, pgn, &fpn, caller) != 0)
		return -1; /* invalid page access */

	int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

	MEMPHY_read(caller->mram, phyaddr, data);

	return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
	int pgn = PAGING_PGN(addr);
	int off = PAGING_OFFST(addr);
	int fpn;

	/* Get the page to MEMRAM, swap from MEMSWAP if needed */
	if (pg_getpage(mm, pgn, &fpn, caller) != 0)
		return -1; /* invalid page access */

	int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

	MEMPHY_write(caller->mram, phyaddr, value);

	return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data)
{
	struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);

	struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

	if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
		return -1;

	pg_getval(caller->mm, currg->rg_start + offset, data, caller);

	return 0;
}

/*pgwrite - PAGING-based read a region memory */
int pgread(
	struct pcb_t *proc, // Process executing the instruction
	uint32_t source,	// Index of source register
	uint32_t offset,	// Source address = [source] + [offset]
	uint32_t destination)
{
	BYTE data;
	int vmaid = 0;
	int val = __read(proc, vmaid, source, offset, &data);

	destination = (uint32_t)data;
#ifdef IODUMP
	printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
	print_pgtbl(proc, 0, -1); // print max TBL
#endif
	MEMPHY_dump(proc->mram);
#endif

	return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value)
{
	struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);

	struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

	if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
		return -1;

	pg_setval(caller->mm, currg->rg_start + offset, value, caller);

	return 0;
}

/*pgwrite - PAGING-based write a region memory */
int pgwrite(
	struct pcb_t *proc,	  // Process executing the instruction
	BYTE data,			  // Data to be wrttien into memory
	uint32_t destination, // Index of destination register
	uint32_t offset)
{
#ifdef IODUMP
	printf("write region=%d offset=%d value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
	print_pgtbl(proc, 0, -1); // print max TBL
#endif
	MEMPHY_dump(proc->mram);
#endif

	return __write(proc, 0, destination, offset, data);
}

/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
	int pagenum, fpn;
	uint32_t pte;

	for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
	{
		pte = caller->mm->pgd[pagenum];

		if (PAGING_PAGE_PRESENT(pte))
		{
			fpn = PAGING_FPN(pte);
			MEMPHY_put_freefp(caller->mram, fpn);
		}
		else
		{
			fpn = PAGING_SWP(pte);
			MEMPHY_put_freefp(caller->active_mswp, fpn);
		}
	}

	return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, int size, int alignedsz)
{
	struct vm_rg_struct *newrg;
	struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

	newrg = malloc(sizeof(struct vm_rg_struct));

	newrg->rg_start = cur_vma->sbrk;
	newrg->rg_end = newrg->rg_start + size;

	return newrg;
}

/* validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, int vmastart, int vmaend)
{
	// struct vm_area_struct *vma = caller->mm->mmap;
	struct vm_area_struct *cur_vma = get_vma_by_num(caller, vmaid);
	struct vm_area_struct *lst_vma = caller->mm->mmap;

	/* TODO DONE: validate the planned memory area is not overlapped */

	/* Check if addresses are in zone of current vma */
	if (vmastart < cur_vma->vm_start || vmastart >= cur_vma->vm_end)
		return -1;
	if (vmaend > vmastart && vmaend <= cur_vma->vm_end)
		return 0;

	/* If that is not the above case,
	 * the end address must be outside of current vma,
	 * then we'll perform overlapped address checking
	 */
	struct vm_area_struct *vmait;

	/* Iterate through the list of vma */
	for (vmait = lst_vma; vmait; vmait = vmait->vm_next)
	{
		/* Do not do the checking for the current vma */
		if (vmait == cur_vma)
			continue;

		/* Check if the end address steps into other vma */
		if (vmaend > vmait->vm_start && vmaend <= vmait->vm_end)
			return -1;
	}

	/* The checking is done and the addresses are valid,
	 * we'll extend current vma after this
	 */
	return 0;
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size
 *
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, int inc_sz)
{
	struct vm_rg_struct *newrg = malloc(sizeof(struct vm_rg_struct));
	int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
	int incnumpage = inc_amt / PAGING_PAGESZ;
	struct vm_rg_struct *area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt);
	struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

	unsigned long old_end = cur_vma->vm_end;
	unsigned long old_sbrk = cur_vma->sbrk;

	/* Validate overlap of obtained region */
	if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_start + inc_amt) < 0)
	{ /*Overlap and failed allocation */
		free(area);
		return -1;
	}

	/* Update new limit for current vma */
	if (cur_vma->sbrk < area->rg_end)
		cur_vma->sbrk = area->rg_end;
	if (cur_vma->vm_end < area->rg_start + inc_amt)
		cur_vma->vm_end = area->rg_start + inc_amt;

	/* The obtained vm area (only)
	 * now we'll be allocating real ram region
	 */
	// cur_vma->vm_end += inc_sz;
	if (vm_map_ram(caller, area->rg_start, area->rg_end,
				   old_end, incnumpage, newrg) < 0)
	{ /* The mapping has been failed */
		free(area);

		/* Undo the limit update */
		cur_vma->vm_end = old_end;
		cur_vma->sbrk = old_sbrk;

		return -1;
	}

	return 0;
}

/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, int *retpgn)
{
	struct pgn_t *pg = mm->fifo_pgn;

	/* TODO DONE: Implement the theorical mechanism to find the victim page */

	/* In case there is no free page */
	if (pg == NULL)
		return -1;

	retpgn = pg->pgn;
	mm->fifo_pgn = pg->pg_next;

	free(pg);

	return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
	struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

	struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;

	if (rgit == NULL)
		return -1;

	/* Probe unintialized newrg */
	newrg->rg_start = newrg->rg_end = -1;

	/* Traverse on list of free vm region to find a fit space */
	while (rgit != NULL)
	{
		if (rgit->rg_start + size <= rgit->rg_end)
		{ /* Current region has enough space */
			newrg->rg_start = rgit->rg_start;
			newrg->rg_end = rgit->rg_start + size;

			/* Update left space in chosen region */
			if (rgit->rg_start + size < rgit->rg_end)
			{
				rgit->rg_start = rgit->rg_start + size;
			}
			else
			{ /*Use up all space, remove current node */
				/*Clone next rg node */
				struct vm_rg_struct *nextrg = rgit->rg_next;

				/*Cloning */
				if (nextrg != NULL)
				{
					rgit->rg_start = nextrg->rg_start;
					rgit->rg_end = nextrg->rg_end;

					rgit->rg_next = nextrg->rg_next;

					free(nextrg);
				}
				else
				{								   /*End of free list */
					rgit->rg_start = rgit->rg_end; // dummy, size 0 region
					rgit->rg_next = NULL;
				}
			}
		}
		else
		{
			rgit = rgit->rg_next; // Traverse next rg
		}
	}

	if (newrg->rg_start == -1) // new region not found
		return -1;

	return 0;
}

// #endif
