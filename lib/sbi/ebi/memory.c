#include <sbi/ebi/memory.h>
#include <sbi/ebi/memutil.h>
#include <sbi/sbi_string.h>
#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>

section_t memory_pool[MEMORY_POOL_SECTION_NUM];
spinlock_t memory_pool_lock;

#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic push
static void dump_section_ownership()
{
	sbi_debug("Nothing\n");
}
#pragma GCC diagnostic pop

// TODO Platform-specific instructions
void flush_dcache_range(uintptr_t start, uintptr_t end)
{
	asm volatile(".long 0xFC000073"); // cflush.d.l1 zero
}

void invalidate_dcache_range(uintptr_t start, uintptr_t end)
{
	asm volatile(".long 0xFC200073"); // cdiscard.d.l1 zero
}

inline void flush_tlb(void)
{
	asm volatile("sfence.vma");
}

void init_memory_pool(void)
{
	int i;
	section_t *sec;

	for_each_section_in_pool(memory_pool, sec, i)
	{
		sec->sfn =
			(MEMORY_POOL_START + i * SECTION_SIZE) >> SECTION_SHIFT;
		sec->owner = -1;
	}
	SPIN_LOCK_INIT(&memory_pool_lock);

	sbi_debug("memory pool init successed!"
		  "start = 0x%lx, end = 0x%lx, num = %lu\n",
		  MEMORY_POOL_START, MEMORY_POOL_END, MEMORY_POOL_SECTION_NUM);
}

uintptr_t alloc_section_for_enclave(enclave_context_t *ectx, uintptr_t va)
{
	int i;
	section_t *sec, *tmp;
	uintptr_t eid;
	uintptr_t ret = 0;
	region_t smallest, avail;
	int tried_flag = 0;

	if (!ectx) {
		sbi_error("Context is NULL!\n");
		return 0;
	}
	eid = ectx->id;

	// 0. (optimization) check whether there is any section available
	if (eid == 0) { // From Host OS
		return alloc_section_for_host_os();
	}

	// 1. Look for available sections adjacent to allocated
	//    sections owned by the enclave. If found, update PMP config
	//    and return the pa of the section
	spin_lock(&memory_pool_lock);
	for_each_section_in_pool(memory_pool, sec, i)
	{
		if (sec->owner != eid)
			continue;

		// left neighbor
		if (i >= 1) {
			tmp = sfn_to_section(sec->sfn - 1);
			if (tmp->owner < 0) {
				ret = tmp->sfn;
				goto found;
			}
		}

		// right neighbor
		if (i <= MEMORY_POOL_SECTION_NUM - 1) {
			tmp = sfn_to_section(sec->sfn + 1);
			if (tmp->owner < 0) {
				ret = tmp->sfn;
				goto found;
			}
		}
	}
	spin_unlock(&memory_pool_lock);

	// 2. If no such section exists, then check whether the PMP resource
	//    has run out. If not, allocate a new section for the enclave
	if (get_avail_pmp_count(ectx) > 0) {
		sec = find_available_section();
		if (!sec) {
			sbi_error("Out of memory!\n");
			return 0;
		}
		ret = sec->sfn;
		goto found;
	}

	// 3. If PMP resource has run out, find the smallest contiguous memory
	//    region owned by the enclave. Assume the region is of N sections.
	//    Look for an available memory region that consists of more than
	//    N + 1 sections. If found, perform section migration and allocate
	//    a section.
try_find:
	if (tried_flag) {
		return 0;
	}
	smallest = find_smallest_region(eid);
	avail	 = find_avail_region_larger_than(smallest.length);
	if (avail.length) {
		section_migration(smallest.sfn, avail.sfn);
		ret = smallest.sfn + smallest.length;
		goto found;
	}

	// 4. If still not found, do page compaction, then repeat step 3.
	page_compaction();
	tried_flag = 1;
	sbi_debug("After compaction\n");
	// repeat step 3
	goto try_find;

found:
	set_section_zero(ret);
	update_section_info(ret, eid, va);
	// PMP

	return ret << SECTION_SHIFT;
}

void free_section_for_enclave(int eid)
{
	int i;
	section_t *sec;

	dump_section_ownership();

	spin_lock(&memory_pool_lock);
	sbi_debug("Got memory pool lock\n");
	for_each_section_in_pool(memory_pool, sec, i)
	{
		if (sec->owner == eid) {
			free_section(sec->sfn);
		}
	}
	spin_unlock(&memory_pool_lock);

	dump_section_ownership();
}

int section_migration(uintptr_t src_sfn, uintptr_t dst_sfn)
{
	section_t *src_sec	  = sfn_to_section(src_sfn);
	section_t *dst_sec	  = sfn_to_section(dst_sfn);
	uintptr_t src_pa	  = src_sfn << SECTION_SHIFT;
	uintptr_t dst_pa	  = dst_sfn << SECTION_SHIFT;
	uintptr_t pa_diff	  = dst_pa - src_pa;
	uintptr_t eid		  = src_sec->owner;
	uintptr_t linear_start_va = src_sec->va;
	enclave_context_t *ectx	  = eid_to_context(eid);
	char is_base_module	  = 0;
	uintptr_t *pt_root_addr, *offset_addr;
	inverse_map_t *inv_map_addr;
	uintptr_t pt_root;
	uintptr_t satp, offset;
	int i;
	inverse_map_t *inv_map_entry;

	sbi_debug("src_pa = 0x%lx, dst_pa = px%lx, pa_diff = 0x%lx\n", src_pa,
		  dst_pa, pa_diff);
	sbi_debug("eid = 0x%lx\n", eid);
	sbi_debug("linear_start_va = 0x%lx\n", linear_start_va);

	if (eid < 0 || ectx == NULL) {
		sbi_error("Invalid EID or context!\n");
		return 0;
	}

	if (dst_sec->owner >= 0) {
		sbi_error("Destination section already occupied!\n");
		return 0;
	}

	pt_root_addr = (uintptr_t *)ectx->pt_root_addr;
	inv_map_addr = (inverse_map_t *)ectx->inverse_map_addr;
	offset_addr  = (uintptr_t *)ectx->offset_addr;
	pt_root	     = *pt_root_addr;

	// 1. judge whether the section contains a base module
	if (src_sfn == SECTION_DOWN((uintptr_t)pt_root_addr) >> SECTION_SHIFT) {
		is_base_module = 1;
	}

	// 2. Copy section content, set section VA&owner
	sbi_memcpy((void *)dst_pa, (void *)src_pa, SECTION_SIZE);
	dst_sec->owner = eid;
	dst_sec->va    = linear_start_va;

	// 3. For base module, calculate the new PA of pt_root,
	//    inv_map, and va_pa_offset.
	//    Update the enclave context, and their value accordingly
	if (is_base_module) {
		ectx->pt_root_addr += pa_diff;
		ectx->inverse_map_addr += pa_diff;
		ectx->offset_addr += pa_diff;
		ectx->pa += pa_diff;
		ectx->drv_list += pa_diff;

		pt_root_addr = (uintptr_t *)ectx->pt_root_addr;
		inv_map_addr = (inverse_map_t *)ectx->inverse_map_addr;
		offset_addr  = (uintptr_t *)ectx->offset_addr;

		// Update pt_root
		*pt_root_addr += pa_diff;
		pt_root = *pt_root_addr;
		satp	= pt_root >> EPAGE_SHIFT;
		satp |= (uintptr_t)SATP_MODE_SV39 << SATP_MODE_SHIFT;
		csr_write(CSR_SATP, satp);
		*offset_addr -= pa_diff;
	}

	// 4. Update page table
	//	a. update tree PTE
	//	b. update leaf PTE
	//		For each PA in the section:
	//		(1). updates its linear map pte
	//		(2). check the inverse map, update the PTE if PA is
	//		     in the table and update the inverse map
	update_tree_pte(pt_root, pa_diff);
	for (offset = 0; offset < SECTION_SIZE; offset += EPAGE_SIZE) {
		update_leaf_pte(pt_root, linear_start_va + offset,
				dst_pa + offset);
	}

	for (offset = 0; offset < SECTION_SIZE; offset += EPAGE_SIZE) {
		inv_map_entry = NULL;
		for (i = 0; inv_map_addr[i].pa && i < INVERSE_MAP_ENTRY_NUM;
		     ++i) {
			if (inv_map_addr[i].pa == src_pa + offset) {
				break;
			}
		}
		if (inv_map_entry) {
			for (i = 0; i < inv_map_entry->count; ++i) {
				update_leaf_pte(pt_root,
						inv_map_entry->va +
							i * EPAGE_SIZE,
						inv_map_entry->pa + pa_diff +
							i * PAGE_SIZE);
			}
			inv_map_entry->pa += pa_diff;
		}
	}

	// 5. Free the source sectino
	spin_lock(&memory_pool_lock);
	free_section(src_sfn);
	spin_unlock(&memory_pool_lock);

	// 6. Flush TLB and D-cache
	flush_tlb();
	flush_dcache_range(dst_pa, dst_pa + SECTION_SIZE);
	invalidate_dcache_range(src_pa, src_pa + SECTION_SIZE);
	if (!is_base_module) {
		flush_dcache_range(pt_root,
				   pt_root + PAGE_DIR_POOL * EPAGE_SIZE);
	}

	return dst_sfn;
}

void memcpy_from_user(uintptr_t maddr, uintptr_t uaddr, uintptr_t size,
		      uintptr_t mepc)
{
	while (size > 0) {
		/* 8 bytes per iter */
		if (size >= 8) {
			*(uint64_t *)maddr =
				load_uint64_t((uint64_t *)uaddr, mepc);
			maddr += 8;
			uaddr += 8;
			size -= 8;
		} else if (size >= 4) {
			*(uint32_t *)maddr =
				load_uint32_t((uint32_t *)uaddr, mepc);
			maddr += 4;
			uaddr += 4;
			size -= 4;
		} else {
			*(uint8_t *)maddr =
				load_uint8_t((uint8_t *)uaddr, mepc);
			// sbi_printf("%c\n", *(char *)maddr);
			++maddr;
			++uaddr;
			--size;
		}
	}
}

void debug_memdump(uintptr_t addr, size_t size)
{
	uint32_t *ptr = (uint32_t *)addr;
	while (size--) {
		sbi_debug("%16p : %8x\n", ptr, *ptr);
		ptr++;
	}
}
