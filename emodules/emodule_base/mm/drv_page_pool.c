#include "drv_page_pool.h"
#ifndef __ASSEMBLER__
#include "../drv_mem.h"
#endif
#include "page_table.h"
#include "../drv_util.h"
#include <sbi/sbi_ecall_interface.h>

/* SPA alway return ACCESSABLE address instead of raw physical address!!!! */

page_list_t page_pools[NUM_POOL];
static uintptr_t alloc_mem_from_m(struct page_list *pool);

// valid before mmu turned on
static void dump_mem_pool(struct page_list *pool)
{
	print_color("[S mode dump_mem_pool]start---------------------------");
	printd("pool %p\n", pool);
	uintptr_t cur = pool->head;
	int cnt	      = 1;
	while (cur && cnt <= 36) {
		printd("0x%x ", cur);
		if (!read_csr(satp))
			cur -= ENC_VA_PA_OFFSET;
		cur = NEXT_PAGE(cur);
		if (cnt++ % 18 == 0)
			printd("\n");
	}
	printd("\n");
	print_color("[S mode dump_mem_pool]end-----------------------------");
}

uintptr_t get_va_pa_offset()
{
	uintptr_t satp = read_csr(satp);
	/* if paging already enabled, add offset */
	if (satp & ((uintptr_t)SATP_MODE_SV39 << SATP_MODE_SHIFT))
		return ENC_VA_PA_OFFSET;
	return 0;
}

uintptr_t va_pa_offset_no_mmu()
{
	return ENC_VA_PA_OFFSET - get_va_pa_offset();
}

static uintptr_t get_phys_addr(uintptr_t va)
{
	return read_csr(satp) ? get_pa(va) : (va - ENC_VA_PA_OFFSET);
}

// this function will be used both before and after mmu gets turned on
void __page_pool_put(uintptr_t pa, struct page_list *pool)
{
	uintptr_t prev;
	uintptr_t section_offset = pa - SECTION_DOWN(pa);
	uintptr_t va		 = va_top + section_offset;
	uintptr_t accessible_va	 = va - va_pa_offset_no_mmu();

	if (!LIST_EMPTY(pool)) {
		prev		= pool->tail - va_pa_offset_no_mmu();
		NEXT_PAGE(prev) = va;
	} else {
		em_debug("List empty, head set to 0x%lx, va_top: 0x%lx\n", va,
			 va_top);
		pool->head = va;
	}

	NEXT_PAGE(accessible_va) = 0;
	pool->tail		 = va;
	pool->count++;
}

// returns virtual addr of the page, -1 on failure
// if Out Of Memory, try allocate a memory section from M mode
uintptr_t __page_pool_get(struct page_list *pool)
{
	uintptr_t page, ret, next;
	if (LIST_EMPTY(pool)) {
		em_debug("Pool tail = 0x%lx\n", pool->tail);
		ret = alloc_mem_from_m(pool);
		if (!ret) {
			em_error("Ret=%d\n", ret);
			return -1;
		}
	}
	page = pool->head - va_pa_offset_no_mmu();
	ret  = pool->head;
	next = NEXT_PAGE(page);

	pool->head = next;
	pool->count--;

	return ret;
}

// should be invoked before MMU is turned on (only once)
void page_pool_init(uintptr_t base, size_t size, char id)
{
	em_debug("Initializing page pool %d\n", (int)id);

	uintptr_t cur;
	page_list_t *pool = page_pools + id;
	LIST_INIT(pool);
	for (cur = base; cur < base + size; cur += EPAGE_SIZE) {
		__page_pool_put(cur, pool);
	}

	dump_mem_pool(pool);
}

void page_pool_put(uintptr_t addr, char id)
{
	__page_pool_put(addr, page_pools + id);
}

uintptr_t page_pool_get_pa(char id)
{
	uintptr_t page = __page_pool_get(page_pools + id);
	if (page == -1)
		return -1;
	return get_phys_addr(page);
}

uintptr_t page_pool_get_pa_zero(char id)
{
	uintptr_t page = __page_pool_get(page_pools + id);
	uintptr_t ret, acce = page - va_pa_offset_no_mmu();
	if (page == -1)
		return -1;
	my_memset((char *)acce, 0, EPAGE_SIZE);
	ret = get_phys_addr(page);
	return ret;
}

uintptr_t page_pool_avail(char id)
{
	return (page_pools + id)->count;
}

// alloc memory from memory manager
// return the start physical address on success; 0 on failure
static uintptr_t alloc_mem_from_m(struct page_list *pool)
{
	uintptr_t addr, size;
	unsigned int pool_size;
	SBI_CALL5(SBI_EXT_EBI, va_top, 0, 0, SBI_EXT_EBI_MEM_ALLOC);
	asm volatile("mv %0, a1" : "=r"(addr)); // return value
	asm volatile("mv %0, a2" : "=r"(size));
	em_debug("mem alloc result: allocated section pa: 0x%lx, size: 0x%lx\n",
		 addr, size);

	if (!addr) {
		em_error("addr is NULL\n");
		return 0;
	}

	// linearly map the allocated memory by VA/PA offset
	em_debug("va_top = 0x%lx\n", va_top);
	map_page(va_top, addr, size >> EPAGE_SHIFT, PTE_V | PTE_W | PTE_R);

	// test
	// SBI_CALL5(0xdeadbeef, addr, 0, 0, 0);

	// put the allocated memory into mem pool
	for (uintptr_t page = addr; page < addr + size; page += EPAGE_SIZE) {
		__page_pool_put(page, pool);
	}

	if (LIST_EMPTY(pool))
		em_error("Pool is empty?\n");

	pool_size = pool->count;
	// printd("[S mode alloc_mem_from_m] pool size is now: 0x%x\n", pool_size);

	va_top += EMEM_SIZE;

	return addr;
}