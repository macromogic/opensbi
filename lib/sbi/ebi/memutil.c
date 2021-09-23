#include <sbi/ebi/memory.h>
#include <sbi/ebi/memutil.h>
#include <sbi/riscv_locks.h>
#include <sbi/sbi_string.h>
#include <sbi/riscv_encoding.h>

int compacted = 0;

uint8_t load_uint8_t(const uint8_t *addr, uintptr_t mepc)
{
	register uintptr_t __mepc asm("a2") = mepc;
	register uintptr_t __mstatus asm("a3");
	uint8_t val;
	asm("csrrs %0, mstatus, %3\n"
	    "lbu %1, %2\n"
	    "csrw mstatus, %0"
	    : "+&r"(__mstatus), "=&r"(val)
	    : "m"(*addr), "r"(MSTATUS_MPRV), "r"(__mepc));
	return val;
}

uint32_t load_uint32_t(const uint32_t *addr, uintptr_t mepc)
{
	register uintptr_t __mepc asm("a2") = mepc;
	register uintptr_t __mstatus asm("a3");
	uint32_t val;
	asm("csrrs %0, mstatus, %3\n"
	    "lw %1, %2\n"
	    "csrw mstatus, %0"
	    : "+&r"(__mstatus), "=&r"(val)
	    : "m"(*addr), "r"(MSTATUS_MPRV), "r"(__mepc));
	return val;
}

uint64_t load_uint64_t(const uint64_t *addr, uintptr_t mepc)
{
	return load_uint32_t((uint32_t *)addr, mepc) +
	       ((uint64_t)load_uint32_t((uint32_t *)addr + 1, mepc) << 32);
}

section_t *find_available_section()
{
	uintptr_t ret_sfn = 0;
	section_t *sec = NULL;
	region_t reg = find_largest_avail();

	if (reg.length == 0) {
		sbi_printf("[M mode find_avail_section] OOM!\n");
		return NULL;
	}

	ret_sfn = reg.sfn + (reg.length >> 1);
	sec = sfn_to_section(ret_sfn);
	
	return sec;
}

uintptr_t alloc_section_for_host_os()
{
	int i;
	section_t *sec;

	spin_lock(&memory_pool_lock);
	for_each_section_in_pool_rev(memory_pool, sec, i)
	{
		if (sec->owner == 0)
			continue;
		spin_unlock(&memory_pool_lock);

		if (sec->owner > 0) {
			section_t *migrate_to = find_available_section();
			section_migration(sec->sfn, migrate_to->sfn);
		}

		update_section_info(sec->sfn, 0, 0);
		return sec->sfn << SECTION_SHIFT;
	}

	// should never reach here
	spin_unlock(&memory_pool_lock);
	sbi_error("Out of memory!\n");
	while(1);
	return 0;
}

int get_avail_pmp_count(enclave_context_t *ectx)
{
	int count = 0;

	if (!ectx) {
		sbi_error("Context is NULL!\n");
		return -1;
	}

	for (int i = 0; i < PMP_REGION_MAX; i++) {
		if (!ectx->pmp_reg->used)
			count++;
	}

	return count;
}

region_t find_largest_avail()
{
	int i;
	section_t *sec;
	uintptr_t head = 0, tail = 0; // sfn
	int hit = 0; // state machine flag
	int max = 0;
	int len;
	region_t ret = {0};

	spin_lock(&memory_pool_lock);
	for_each_section_in_pool(memory_pool, sec, i) {
		if (sec->owner >= 0 && hit) {
			len = (int)(tail - head + 1);
			if (len > max) {
				max = len;
				ret.sfn = head;
				ret.length = max;
			}
			hit = 0;
		}

		if (sec->owner < 0 && !hit) {
			head = sec->sfn;
			tail = sec->sfn;
			hit = 1;
			continue;
		}

		if (sec->owner < 0 && hit) {
			tail++;
		}
	}

	// if the region is at the end of the section list
	if (hit) {
		len = (int)(tail - head + 1);
		if (len > max) {
			max = len;
			ret.sfn = head;
			ret.length = max;
		}
	}
	// sbi_printf("[M mode find_largest_avail] largest: at 0x%lx, %d sections\n",
			// ret.sfn << SECTION_SHIFT, ret.length);
	spin_unlock(&memory_pool_lock);

	return ret;
}

region_t find_smallest_region(int eid)
{
	int i;
	section_t *sec;
	uintptr_t head = 0, tail = 0; // sfn
	int hit = 0; // state machine flag
	int min = MEMORY_POOL_SECTION_NUM + 1;
	int len;
	region_t ret = {0};

	for_each_section_in_pool(memory_pool, sec, i) {
		if (sec->owner != eid && hit) {
			len = (int)(tail - head + 1);
			if (len < min) {
				min = len;
				ret.sfn = head;
				ret.length = min;
			}
			hit = 0;
		}

		if (sec->owner == eid && !hit) {
			head = sec->sfn;
			tail = sec->sfn;
			hit = 1;
			continue;
		}

		if (sec->owner == eid && hit) {
			tail++;
		}
	}

	// if the region is at the end of the section list
	if (hit) {
		len = (int)(tail - head + 1);
		if (len < min) {
			min = len;
			ret.sfn = head;
			ret.length = min;
		}
	}

	// sbi_printf("[M mode find_smallest_region] %d smallest: at 0x%lx, %d sections\n",
			// eid, ret.sfn << SECTION_SHIFT, ret.length);

	return ret;
}
region_t find_avail_region_larger_than(int length)
{
	int i;
	section_t *sec;
	uintptr_t head = 0, tail = 0;
	int hit = 0;
	int len;
	region_t ret;

	sbi_memset((void *)&ret, 0, sizeof(region_t));

	for_each_section_in_pool(memory_pool, sec, i) {
		if (sec->owner >= 0 && hit) {
			len = (int)(tail - head + 1);
			if (len > length) {
				ret.length = len;
				ret.sfn = head;
				return ret;
			}
			hit = 0;
		}

		if (sec->owner < 0 && !hit) {
			head = sec->sfn;
			tail = sec->sfn;
			hit = 1;
			continue;
		}

		if (sec->owner < 0 && hit) {
			tail++;
		}
	}

	// if the region is at the end of the section list
	if (hit) {
		len = (int)(tail - head + 1);
		if (len > length) {
			ret.length = len;
			ret.sfn = head;
			return ret;
		}
	}

	// got here means not found
	return ret;
}

void dump_utilization_rate()
{
	int i;
	section_t *sec;
	int count = 0;

	for_each_section_in_pool(memory_pool, sec, i) {
		if (sec->owner > 0) {
			count++;
		}
	}

	sbi_printf("utilization rate: %d/%d\n", count, (int)MEMORY_POOL_SECTION_NUM);
}

void page_compaction()
{
	int i;
	section_t *sec;
	section_t *tmp;
	int done = 1;
	static int count = 0;

	dump_section_ownership();

	sbi_printf("[M mode section_compaction]\n");
	dump_utilization_rate();

	for_each_section_in_pool(memory_pool, sec, i) {
		done = 1;
		if (sec->owner < 0) {
			for (int j = 1; i + j < MEMORY_POOL_SECTION_NUM; j++) {
				tmp = sfn_to_section(sec->sfn + j);
				if (tmp->owner > 0) {
					section_migration(tmp->sfn, sec->sfn);
					done = 0;
					break;
				}
			}

			if (done) {
				count++;
				dump_section_ownership();
				return;
			}
			
		}
	}
}

void update_tree_pte(uintptr_t root, uintptr_t pa_diff)
{
	pte_t *entry = (pte_t *)root;
	uintptr_t next_level;
	int i;

	// FIXME Hardcoded number
	for (i = 0; i < 512; ++i, ++entry) {
		// Leaf PTE
		if (entry->pte_r || entry->pte_w || entry->pte_x) {
			return;
		}
		// Empty PTE
		if (!entry->pte_v) {
			continue;
		}

		entry->ppn += pa_diff >> EPAGE_SHIFT;
		next_level = (uintptr_t)entry->ppn << EPAGE_SHIFT;
		update_tree_pte(next_level, pa_diff);
	}
}

static pte_t *get_pte(pte_t *root, uintptr_t va)
{
	uintptr_t layer_offset[] = { (va & MASK_L2) >> 30, (va & MASK_L1) >> 21,
				     (va & MASK_L0) >> 12 };
	pte_t *tmp_entry;
	int i;
	for (i = 0; i < 3; ++i) {
		tmp_entry = &root[layer_offset[i]];
		if (!tmp_entry->pte_v) {
			return NULL;
		}
		if ((tmp_entry->pte_r | tmp_entry->pte_w | tmp_entry->pte_x)) {
			return tmp_entry;
		}
		root = (pte_t *)((uintptr_t)tmp_entry->ppn << EPAGE_SHIFT);
	}
	return NULL;
}

void update_leaf_pte(uintptr_t root, uintptr_t va, uintptr_t pa)
{
	pte_t *entry = get_pte((pte_t *)root, va);
	if (entry) {
		entry->ppn = pa >> EPAGE_SHIFT;
	}
}

void set_section_zero(uintptr_t sfn)
{
	char *s = (char *)(sfn << SECTION_SHIFT);
	// sbi_debug("section start: %p\n", s);
	sbi_memset(s, 0, SECTION_SIZE);
	// sbi_debug("setting zero done\n");
}

void update_section_info(uintptr_t sfn, int owner, uintptr_t va)
{
	spin_lock(&memory_pool_lock);
	section_t *sec = sfn_to_section(sfn);
	spin_unlock(&memory_pool_lock);

	sec->owner = owner;
	sec->va	   = va;
}

void free_section(uintptr_t sfn)
{
	section_t *sec = sfn_to_section(sfn);

	if (sec->owner < 0)
		return;

	// sbi_debug("freeing section 0x%lx\n", sfn);

	// clear memory
	set_section_zero(sfn);

	// configure pmp. previous owner may no longer access it.
	// TODO PMP operation
	// pmp_withdraw(sec);

	sec->owner = -1;
	sec->va	   = 0;
}