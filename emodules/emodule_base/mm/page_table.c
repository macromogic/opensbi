#include <stdint.h>
#include "page_table.h"
#include "drv_page_pool.h"
#include "../drv_mem.h"
#include "../drv_util.h"

#define PAGE_SIZE 4096

static uintptr_t page_directory_pool; // always store pa in

uintptr_t ENC_VA_PA_OFFSET;
inverse_map_t inv_map[INVERSE_MAP_ENTRY_NUM];

#define DEBUG_CONDITION(cond) int debug = (cond) ? 1 : 0;
#define DEBUG if (debug)

// accessible addr to pa
static inline uintptr_t acce_to_phys(uintptr_t acce_addr)
{
	return read_csr(satp) ? (acce_addr - ENC_VA_PA_OFFSET) : acce_addr;
}

/**
 * insert a va-pa pair to page table, maintained via a trie
 * @param t a trie to maintain used page directory in a pool, should be `static trie address_trie`
 * @param va the virtual address
 * @param pa the physical address corresponding to va
 * @param len the number of levels should be used. Note, it can be smaller than 3, which indicates a huge page
 * @param attr pte attribution, reserved for future
 * @return
 */
static uintptr_t trie_get_or_insert(trie_t *t, const uintptr_t va,
				    const uintptr_t pa, const int len,
				    const uintptr_t attr)
{
	uint32_t p = 0, i = 0;
	page_directory *page_table = (page_directory *)get_page_table_root();
	/*
     * [L2, L1, L0] PPN for each level, used fot trie to get offset of 
     * page_directory_pool
     * Note: l[0] = L1, l[2] = l[0]
     */

	uintptr_t l[] = { (va & MASK_L2) >> 30, (va & MASK_L1) >> 21,
			  (va & MASK_L0) >> 12 };
	pte_t *tmp_pte;
	pte_t *debug_pte;

	// for a three level page table, only two PPNs need to point to next level
	for (; i < len - 1; i++) {
		if (!t->next[p][l[i]]) {
			t->next[p][l[i]] = ++t->cnt;
			em_debug("\033[1;33mpage cnt:%d\033[0m\n", t->cnt);

			tmp_pte = &page_table[p][l[i]];
			tmp_pte->ppn =
				acce_to_phys(
					(uintptr_t)&page_table[t->cnt][0]) >>
				12;
			tmp_pte->pte_v = 1;
		}

		p = t->next[p][l[i]];
	}
	// set items for the leaf page table entry
	tmp_pte	     = &page_table[p][l[len - 1]];
	tmp_pte->ppn = pa >> 12;
	if (len == 2) {
		tmp_pte->ppn = (tmp_pte->ppn | MASK_OFFSET) ^ MASK_OFFSET;
	}
	tmp_pte->pte_v = tmp_pte->pte_g = 1;
	// tmp_pte->pte_v = 1;
	if (attr & PTE_U) {
		tmp_pte->pte_u = 1;
	}
	if (attr & PTE_W || attr & PTE_D) {
		tmp_pte->pte_d = tmp_pte->pte_w = 1;
	}
	if (attr & PTE_R || attr & PTE_A) {
		tmp_pte->pte_a = tmp_pte->pte_r = 1;
	}
	if (attr & PTE_X) {
		tmp_pte->pte_x = 1;
	}

	return *((uintptr_t *)tmp_pte);
}

static uintptr_t page_directory_insert(uintptr_t va, uintptr_t pa, int levels,
				       uintptr_t attr)
{
	trie_t *t   = (trie_t *)get_trie_root();
	uintptr_t p = (uintptr_t)trie_get_or_insert(t, va, pa, levels, attr);
	return p;
}

void set_page_table_root(
	uintptr_t pt_root) // should only be invoked before mmu enabled
{
	page_directory_pool = pt_root;
}

inline uintptr_t get_page_table_root() // returns accessible addr
{
	return page_directory_pool + get_va_pa_offset();
}

// returns accessible value; trie is always right behind the page table
inline uintptr_t get_trie_root()
{
	return get_page_table_root() + EPAGE_SIZE * PAGE_DIR_POOL;
}

uintptr_t
get_page_table_root_pointer_addr() // should only be invoked before mmu enabled
{
	return (uintptr_t)&page_directory_pool;
}

static inline void flush_page_table_cache_and_tlb()
{
	uintptr_t pt_root = page_directory_pool;
	flush_dcache_range(pt_root,
			   pt_root + PAGE_DIR_POOL * EPAGE_SIZE +
				   PAGE_DIR_POOL * 4 * 512 +
				   EPAGE_SIZE); // flush does not work, why?
	// invalidate_dcache_range(pt_root,
	// 			pt_root + PAGE_DIR_POOL * EPAGE_SIZE
	// 			+ PAGE_DIR_POOL * 4 * 512 + EPAGE_SIZE); // invalidation works, why?
	// asm volatile("fence rw, rw");
	flush_tlb();
}

void print_pte(uintptr_t va)
{
	uintptr_t l[] = { (va & MASK_L2) >> 30, (va & MASK_L1) >> 21,
			  (va & MASK_L0) >> 12 };
	pte_t entry;
	pte_t *root = (void *)get_page_table_root();
	pte_t tmp_entry;
	uintptr_t tmp;
	int i = 0;
	while (1) {
		tmp_entry = root[l[i]];
		if (!tmp_entry.pte_v) {
			em_error("va:0x%lx is not valid!!!\n", va);
			return;
		}
		if ((tmp_entry.pte_r | tmp_entry.pte_w | tmp_entry.pte_x)) {
			break;
		}
		tmp  = tmp_entry.ppn << 12;
		root = (pte_t *)(tmp + get_va_pa_offset());
		i++;
	}
	em_debug("##########PRINT PTE @ %x############\n", va);
	em_debug("PTE stored @ %p\n", root);
	em_debug("PTE ppn: %x\n", tmp_entry.ppn);
	em_debug("PTE pte_v: %x\n", tmp_entry.pte_v);
	em_debug("PTE pte_r: %x\n", tmp_entry.pte_r);
	em_debug("PTE pte_w: %x\n", tmp_entry.pte_w);
	em_debug("PTE pte_x: %x\n", tmp_entry.pte_x);
	em_debug("PTE pte_u: %x\n", tmp_entry.pte_u);
	em_debug("PTE pte_g: %x\n", tmp_entry.pte_g);
	em_debug("PTE pte_a: %x\n", tmp_entry.pte_a);
	em_debug("PTE pte_d: %x\n", tmp_entry.pte_d);
	em_debug("##########PRINT PTE END#################\n");
}

uintptr_t get_pa(uintptr_t va)
{
	uintptr_t l[] = { (va & MASK_L2) >> 30, (va & MASK_L1) >> 21,
			  (va & MASK_L0) >> 12 };
	pte_t entry;
	pte_t *root = (void *)get_page_table_root();
	pte_t tmp_entry;
	uintptr_t tmp;
	int i = 0;
	while (1) {
		// #ifdef EMODULE_GLOBAL_DEBUG
		// 		printd("[S mode get_pa] i = %d, root = %p, OFFSET = 0x%lx\n", i,
		// 		       root, ENC_VA_PA_OFFSET);
		// #endif
		tmp_entry = root[l[i]];
		if (!tmp_entry.pte_v) {
			em_debug("va:0x%lx is not valid!!!\n", va);
			return 0;
		}
		if ((tmp_entry.pte_r | tmp_entry.pte_w | tmp_entry.pte_x)) {
			break;
		}
		tmp  = tmp_entry.ppn << 12;
		root = (pte_t *)(tmp + get_va_pa_offset());
		i++;
	}
	if (i == 2)
		return (tmp_entry.ppn << 12) | (va & 0xfff);
	else if (i == 1)
		return (tmp_entry.ppn >> 9) << 21 | (va & 0x1fffff);
	else {
		return 0;
	}
}

void test_va(uintptr_t va)
{
	uintptr_t *content = (uintptr_t *)va;
	uintptr_t pa	   = get_pa(va);
	em_debug("va: 0x%lx --> pa: 0x%lx\n", va, pa);
	print_pte(va);
	if (read_csr(satp))
		em_debug("content: 0x%lx\n", *content);
}

void map_page(uintptr_t va, uintptr_t pa, size_t n_pages, uintptr_t attr)
{
	pte_t *pt;
	char is_text = 0;

	if (n_pages == 0) {
		return;
	}

	if (va <= 0x100000) // TODO to be modified: used by load_elf
		is_text = 1;
	if (is_text) {
		insert_inverse_map(pa, va, n_pages);
	}

	// while (n_pages >= 512) {
	// 	page_directory_insert(va, pa, 2, attr);
	// 	va += EPAGE_SIZE * 512;
	// 	pa += EPAGE_SIZE * 512;
	// 	n_pages -= 512;
	// }
	while (n_pages > 0) {
		page_directory_insert(va, pa, 3, attr);
		// if (n_pages == 1)
		// 	test_va(va);

		va += EPAGE_SIZE;
		pa += EPAGE_SIZE;
		n_pages--;
	}

	if (read_csr(satp)) {
		flush_page_table_cache_and_tlb();
	}
}

uintptr_t ioremap(pte_t *root, uintptr_t pa, size_t size)
{
	static uintptr_t drv_addr_alloc = 0;
	em_debug("current root address: 0x%lx\n", get_page_table_root());
	size_t n_pages = PAGE_UP(size) >> EPAGE_SHIFT;
	map_page(EDRV_DRV_START + drv_addr_alloc, pa, n_pages,
		 PTE_V | PTE_W | PTE_R | PTE_D | PTE_X);
	uintptr_t cur_addr = EDRV_DRV_START;
	drv_addr_alloc += n_pages << 12;
	return cur_addr;
}

uintptr_t alloc_page(pte_t *root, uintptr_t va, uintptr_t n_pages,
		     uintptr_t attr, char id)
{
	pte_t *pt;
	uintptr_t pa, prev_pa = 0, base_pa = 0;
	inverse_map_t *inv_map_entry;

	em_debug("va = 0x%lx, n = %d\n", va, n_pages);
	while (n_pages >= 1) {
		// pa = spa_get_pa_zero(id);
		pa = page_pool_get_pa(id);
		if (pa == prev_pa + EPAGE_SIZE &&
		    SECTION_DOWN(pa) == SECTION_DOWN(prev_pa)) {
			// inverse_map_add_count(base_pa);
			inv_map_entry->count++;
		} else {
			inv_map_entry = insert_inverse_map(pa, va, 1);
			if (!inv_map_entry) {
				em_error("inv_map_entry is NULL!\n");
				return 0;
			}

			base_pa = pa;
		}
		// printd("[S mode alloc_page] va: 0x%lx, pa: 0x%lx\n", va, pa);
		page_directory_insert(va, pa, 3, attr);
		prev_pa = pa;
		va += EPAGE_SIZE;
		n_pages--;
	}

	dump_inverse_map();

	if (read_csr(satp)) {
		flush_page_table_cache_and_tlb();
	}

	return pa;
}

void check_pte_all_zero()
{
	int i, j;
	pte_t *tmp_pte;
	page_directory *page_table = (page_directory *)get_page_table_root();
	for (i = 0; i < 64; i++) {
		for (j = 0; j < 512; j++) {
			tmp_pte = &page_table[i][j];
			if (*((uintptr_t *)tmp_pte)) {
				em_error("%d:%d not zero!\n", i, j);
				tmp_pte = 0;
			}
		}
	}
}

// look up pa first. if pa exists in the table, update it; otherwise
// insert a new entry
// when updating, count must match with the previous count
// returns the newly inserted entry
inverse_map_t *insert_inverse_map(uintptr_t pa, uintptr_t va, uint32_t count)
{
	int i = 0;

	em_debug("pa: 0x%lx, va: 0x%lx, count: %d\n", pa, va, count);
	for (; inv_map[i].pa && i < INVERSE_MAP_ENTRY_NUM; i++) {
		if (pa == inv_map[i].pa) { // already exists; should update
			em_debug(
				"updating entry; original va: 0x%lx, count: %d\n",
				va, count);
			if (count != inv_map[i].count) {
				// something goes wrong
				em_error(
					"Count does not match! original count: %d\n",
					inv_map[i].count);
				while (1)
					;
				return NULL;
			}
			inv_map[i].va = va;
		}
	}
	if (i == INVERSE_MAP_ENTRY_NUM) { // out of entry
		em_error("NO ENOUGH ENTRY!!!\n");
		return NULL;
	}

	// new entry
	inv_map[i].pa	 = pa;
	inv_map[i].va	 = va;
	inv_map[i].count = count;
	em_debug("New entry\n");

	return &inv_map[i];
}

void inverse_map_add_count(uintptr_t pa)
{
	if (!pa)
		em_error("Invalid pa\n");
	for (int i = 0; i < INVERSE_MAP_ENTRY_NUM; i++) {
		if (pa == inv_map[i].pa) {
			inv_map[i].count++;
			if (inv_map[i].count % 100 == 0)
				em_debug("pa: 0x%lx, new count: %d\n", pa,
					 inv_map[i].count);
			return;
		}
	}
	em_error("Failed!\n");
}

void dump_inverse_map()
{
	em_debug("start-------------------\n");
	for (int i = 0; i < INVERSE_MAP_ENTRY_NUM && inv_map[i].pa; i++) {
		em_debug("%d: pa = %lx, va = %lx, count = %d\n", i,
			 inv_map[i].pa, inv_map[i].va, inv_map[i].count);
	}
	em_debug("end---------------------\n");
}

inline void flush_tlb(void)
{
	asm volatile("sfence.vma");
}