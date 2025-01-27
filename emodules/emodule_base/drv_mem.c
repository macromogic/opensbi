#ifndef __ASSEMBLER__
#include "drv_mem.h"
#endif
#include <sbi/ebi/memory.h>
#include <sbi/sbi_ecall_interface.h>
#include "drv_base.h"
#include "drv_elf.h"
#include "drv_list.h"
#include "mm/drv_page_pool.h"
#include "mm/page_table.h"
#include "drv_util.h"
#include "md2.h"
/* Each Eapp has their own program break */
uintptr_t prog_brk;
uintptr_t drv_start_va;
uintptr_t va_top;

#define __pa(x) get_pa((x) + ENC_VA_PA_OFFSET)

#define MAP_BASE_SECTION(sec_name, pte_flags)                                  \
	do {                                                                   \
		extern char _##sec_name##_start, _##sec_name##_end;            \
		uintptr_t sec_name##_start =                                   \
			PAGE_DOWN((uintptr_t)&_##sec_name##_start);            \
		uintptr_t sec_name##_end  = (uintptr_t)&_##sec_name##_end;     \
		uintptr_t sec_name##_size = sec_name##_end - sec_name##_start; \
		size_t n_base_##sec_name##_pages =                             \
			PAGE_UP(sec_name##_size) >> EPAGE_SHIFT;               \
		map_page(ENC_VA_PA_OFFSET + sec_name##_start,                  \
			 sec_name##_start, n_base_##sec_name##_pages,          \
			 (pte_flags));                                         \
		em_debug(#sec_name ": 0x%x - 0x%x -> 0x%x\n",                  \
			 sec_name##_start, sec_name##_end,                     \
			 __pa(sec_name##_start));                              \
	} while (0)

static inline void page_map_register()
{
	SBI_CALL5(SBI_EXT_EBI, get_page_table_root_pointer_addr(), &inv_map,
		  &ENC_VA_PA_OFFSET, SBI_EXT_EBI_MAP_REGISTER);
}

void loop_test()
{
	print_color("---------------------- start");
	uintptr_t cycle1, cycle2;
	for (int i = 0; i < 10; i++) {
		int j;
		cycle1 = read_csr(cycle);
		for (j = 0; j < 1000000; j++) {
			asm volatile("addi t4, t4, 1");
		}
		cycle2 = read_csr(cycle);
		em_debug("cycle diff = %ld\n", cycle2 - cycle1);
	}
	print_color("---------------------- end");
}

static inline void attest_payload(void *payload_start, size_t payload_size)
{
	uint64_t begin_cycle, end_cycle;
	uint8_t md2hash[MD2_BLOCK_SIZE];

	begin_cycle = read_csr(cycle);
	md2(payload_start, payload_size, md2hash);
	end_cycle = read_csr(cycle);
	em_debug("MD2: 0x%x in %ld cycles\n", end_cycle - begin_cycle);
}

// Memory mapping setup
static void
init_map_alloc_pages(drv_addr_t *drv_list, uintptr_t page_table_start,
		     size_t page_table_size, uintptr_t usr_avail_start,
		     size_t usr_avail_size, uintptr_t base_avail_start,
		     size_t base_avail_size)
{
	uintptr_t drv_pa_start, drv_pa_end;
	size_t n_drv_pages;
	uintptr_t usr_stack_start;
	size_t n_usr_stack_pages, n_base_stack_pages;
	uintptr_t drv_sp;

	// Drivers
	em_debug("drv_list = 0x%lx\n", drv_list);
	if (drv_list[0].drv_start) {
		drv_pa_start =
			PAGE_DOWN(drv_list[0].drv_start - ENC_VA_PA_OFFSET);
		drv_pa_end  = PAGE_UP(((uintptr_t)drv_list) +
				      MAX_DRV * sizeof(drv_addr_t));
		n_drv_pages = (drv_pa_end - drv_pa_start) >> EPAGE_SHIFT;
		em_debug("drv_pa_end = 0x%x drv_pa_start = 0x%x\n", drv_pa_end,
			 drv_pa_start);
		em_debug("n_drv_pages = %d\n", n_drv_pages);
		map_page(PAGE_DOWN(drv_list[0].drv_start), drv_pa_start,
			 n_drv_pages, PTE_V | PTE_R | PTE_X | PTE_W);
		em_debug("\033[1;33mdrv: 0x%x - 0x%x -> 0x%x\n\033[0m",
			 drv_pa_start, drv_pa_end, __pa(drv_pa_start));
	}

	// Remaining user memory
	map_page(ENC_VA_PA_OFFSET + usr_avail_start, usr_avail_start,
		 usr_avail_size >> EPAGE_SHIFT, PTE_V | PTE_W | PTE_R);
	em_debug("usr.remain: 0x%x - 0x%x -> 0x%x\n", usr_avail_start,
		 usr_avail_start + PAGE_DOWN(usr_avail_size),
		 __pa(usr_avail_start));

	// Allocate user stack (r/w)
	n_usr_stack_pages = (PAGE_UP(EUSR_STACK_SIZE) >> EPAGE_SHIFT) + 1;
	em_debug("User stack needs %d pages\n", n_usr_stack_pages);
	usr_stack_start		= EUSR_STACK_START;
	uintptr_t usr_stack_end = EUSR_STACK_END; // debug use
	em_debug("User stack: 0x%lx - 0x%lx\n", usr_stack_start, usr_stack_end);
	alloc_page(NULL, usr_stack_start, n_usr_stack_pages,
		   PTE_V | PTE_W | PTE_R | PTE_U, IDX_USR);

	// Map pages for base module
	// `.text' section
	MAP_BASE_SECTION(text, PTE_V | PTE_X | PTE_R);
	// Page table (and trie)
	map_page(ENC_VA_PA_OFFSET + page_table_start, page_table_start,
		 (page_table_size) >> EPAGE_SHIFT, PTE_V | PTE_W | PTE_R);
	em_debug("page_table and trie: 0x%x - 0x%x -> 0x%x\n", page_table_start,
		 page_table_size, __pa(page_table_start));
	// `.rodata', `.bss', `.init.data`, `.data' sections
	MAP_BASE_SECTION(rodata, PTE_V | PTE_R);
	MAP_BASE_SECTION(bss, PTE_V | PTE_W | PTE_R);
	MAP_BASE_SECTION(init_data, PTE_V | PTE_W | PTE_R);
	MAP_BASE_SECTION(data, PTE_V | PTE_W | PTE_R);

	// Remaining base memory
	map_page(ENC_VA_PA_OFFSET + base_avail_start, base_avail_start,
		 PAGE_DOWN(base_avail_size) >> EPAGE_SHIFT,
		 PTE_V | PTE_W | PTE_R);
	em_debug("drv.remain: 0x%x - 0x%x -> 0x%x\n", base_avail_start,
		 base_avail_start + PAGE_DOWN(base_avail_size),
		 __pa(base_avail_start));

	// Allocate base stack (r/w)
	n_base_stack_pages = (PAGE_UP(EDRV_STACK_SIZE)) >> EPAGE_SHIFT;
	em_debug("drv stack uses %d pages\n", n_base_stack_pages);
	drv_sp = EDRV_STACK_TOP - EDRV_STACK_SIZE;
	alloc_page(NULL, drv_sp, n_base_stack_pages, PTE_V | PTE_W | PTE_R,
		   IDX_DRV);
	drv_sp += EDRV_STACK_SIZE;
	em_debug("sp: 0x%lx\nPage table root: 0x%lx\n", drv_sp,
		 get_page_table_root());
}

/* Initialize memory for driver, including stack, heap, page table.
 * Memory layout:
 *
 *                     -----------------  HIGH ADDR
 *                       < base memory space >
 * base_avail_start => -----------------
 *                     ^ < trie >
 *                     | trie_size == PAGE_UP(sizeof(trie_t))
 *                     v
 * trie_start =======> -----------------
 *                     ^ < page table >
 *                     | page_table_size == PAGE_UP(PAGE_DIR_POOL * EPAGE_SIZE)
 *                     v
 * page_table_start => -----------------
 *                     ^ < driver list >
 *                     | ~ DRV_MAX * sizeof(drv_addr_t)
 *                     v
 * drv_list =========> -----------------
 *                       < user memory space >
 * usr_avail_start ==> -----------------
 *                     ^ < user payload >
 *                     | PAGE_UP(payload_size)
 *                     v
 * payload_pa_start => -----------------  LOW ADDR
 *
 */
void init_mem(uintptr_t _, uintptr_t id, uintptr_t payload_pa_start,
	      uintptr_t payload_size, drv_addr_t drv_list[MAX_DRV],
	      uintptr_t argc, uintptr_t argv)
{
	uintptr_t page_table_start, page_table_size;
	uintptr_t trie_start, trie_size;
	uintptr_t base_avail_start, base_avail_size;
	uintptr_t usr_avail_start, usr_avail_size;
	uintptr_t satp, sstatus;
	uintptr_t usr_pc;
	int i;
	uintptr_t usr_sp = EUSR_STACK_END;
	uintptr_t drv_sp = EDRV_STACK_TOP;

	// Update VA/PA offset
	ENC_VA_PA_OFFSET = EDRV_VA_START - payload_pa_start;
	em_debug("\033[1;33moffset: 0x%x\n\033[0m", ENC_VA_PA_OFFSET);
	// `va_top' will increase by EMEM_SIZE after `page_pool_init'
	va_top = EDRV_VA_START;
	attest_payload((void *)payload_pa_start, payload_size);
	enclave_id = id;

	// Transform driver addresses into VA
	em_debug("drv_list[0].drv_start = 0x%x\n", drv_list[0].drv_start);
	for (i = 0; i < MAX_DRV && drv_list[i].drv_start; i++) {
		drv_list[i].drv_start += ENC_VA_PA_OFFSET;
		drv_list[i].drv_end += ENC_VA_PA_OFFSET;
		em_debug("drv_list[%d].drv_start: 0x%x, drv_end: 0x%x\n", i,
			 drv_list[i].drv_start, drv_list[i].drv_end);
	}
	drv_addr_list = (drv_addr_t *)((void *)drv_list + ENC_VA_PA_OFFSET);

	// Setup page table, trie and base memory space
	page_table_start =
		PAGE_UP((uintptr_t)drv_list + MAX_DRV * sizeof(drv_addr_t));
	page_table_size = PAGE_UP(PAGE_DIR_POOL * EPAGE_SIZE);
	em_debug("page_table_start = 0x%x\n", page_table_start);
	em_debug("page_table_size = 0x%x\n", page_table_size);
	set_page_table_root(page_table_start);

	trie_start = page_table_start + page_table_size;
	trie_size  = PAGE_UP(sizeof(trie_t));

	base_avail_start = trie_start + trie_size;
	base_avail_size =
		PAGE_DOWN(payload_pa_start + EMEM_SIZE - base_avail_start);
	em_debug("base_avail_start = 0x%x\n", base_avail_start);
	em_debug("base_avail_size = %x\n", base_avail_size);
	page_pool_init(base_avail_start, base_avail_size, IDX_DRV);

	// Setup user memory space
	usr_avail_start = PAGE_UP(payload_pa_start + payload_size);
	usr_avail_size	= PAGE_DOWN(EUSR_MEM_SIZE - payload_size);
	page_pool_init(usr_avail_start, usr_avail_size, IDX_USR);
	em_debug("Initializing user page pool: 0x%x, size: 0x%x\n",
		 usr_avail_start, usr_avail_size);
	em_debug("User page pool initialize done\n");
	check_pte_all_zero(); // Is it necessary?
	em_debug("\033[1;33mroot: 0x%x\n\033[0m", get_page_table_root());

	// Load payload ELF
	usr_pc = elf_load(0, payload_pa_start, IDX_USR, &prog_brk);

	init_map_alloc_pages(drv_list, page_table_start,
			     page_table_size + trie_size, usr_avail_start,
			     usr_avail_size, base_avail_start, base_avail_size);

	// Update `satp', `sstatus', allow S-mode access to U-mode memory
	em_debug("usr sp: 0x%llx\n", usr_sp);
	satp = get_page_table_root() >> EPAGE_SHIFT;
	satp |= (uintptr_t)SATP_MODE_SV39 << SATP_MODE_SHIFT;
	sstatus = read_csr(sstatus);
	sstatus |= SSTATUS_SUM;
	write_csr(sstatus, sstatus);

	// loop_test();

	// Inform M-mode of locations of page table and inverse mapping
	page_map_register();
	va_top += EMEM_SIZE;
	flush_dcache_range(payload_pa_start, payload_pa_start + EMEM_SIZE);
	// asm volatile("fence rw, rw");

	asm volatile("mv a0, %0" ::"r"((uintptr_t)(satp)));
	asm volatile("mv a1, %0" ::"r"((uintptr_t)(drv_sp)));
	asm volatile("mv a2, %0" ::"r"((uintptr_t)(usr_pc)));
	asm volatile("mv a3, %0" ::"r"((uintptr_t)(usr_sp)));
}
