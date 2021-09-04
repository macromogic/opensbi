#pragma once

#include "../drv_util.h"
#include "../drv_mem.h"
#include <sbi/ebi/memory.h>

// #define ENC_PA_START    0x40000000
#define EDRV_VA_START 0xC0000000
#define EDRV_DRV_START 0xD0000000

#define MASK_OFFSET 0xfff
#define MASK_L0 0x1ff000
#define MASK_L1 0x3fe00000
#define MASK_L2 0x7fc0000000

// Pool size for page table itself
// NOTE: when modifying, modify sbi_ecall_ebi_mem.h at the same time!!!!!!!
#define PAGE_DIR_POOL 256

#ifndef __ASSEMBLER__

typedef unsigned long size_t;

typedef struct trie {
	uint32_t next[PAGE_DIR_POOL][512], cnt;
} trie_t;
typedef pte_t page_directory[512];

extern uintptr_t ENC_PA_START;
extern uintptr_t ENC_VA_PA_OFFSET;
extern inverse_map_t inv_map[INVERSE_MAP_ENTRY_NUM];

void map_page(pte_t *root, uintptr_t va, uintptr_t pa, size_t n_pages,
	      uintptr_t attr);
uintptr_t ioremap(pte_t *, uintptr_t, size_t);
uintptr_t alloc_page(pte_t *, uintptr_t, uintptr_t, uintptr_t, char);
uintptr_t get_pa(uintptr_t);
void print_pte(uintptr_t va);
void test_va(uintptr_t va);
void set_page_table_root(uintptr_t pt_root);
uintptr_t get_page_table_root(void);
uintptr_t get_page_table_root_pointer_addr();
uintptr_t get_trie_root();
void all_zero(void);
inverse_map_t *insert_inverse_map(uintptr_t pa, uintptr_t va, uint32_t count);
void inverse_map_add_count(uintptr_t pa);
void dump_inverse_map();

#endif