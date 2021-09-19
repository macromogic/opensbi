#pragma once
#ifndef __ASSEMBLER__
#include <stdio.h>
#include <stdint.h>
#include "../enclave.h"

#define NEXT_PAGE(page) *((uintptr_t *)page)
#define LIST_EMPTY(list) ((list)->count == 0 || (list)->head == 0)
#define LIST_INIT(list)            \
	{                          \
		(list)->count = 0; \
		(list)->head  = 0; \
		(list)->tail  = 0; \
	}

typedef enum { IDX_USR, IDX_DRV, NUM_POOL } mem_pool_idx_t;

#define EMEM_SIZE SECTION_SIZE
#define SECTION_UP(addr) (ROUND_UP(addr, SECTION_SIZE))
#define SECTION_DOWN(addr) ((addr) & (~((SECTION_SIZE)-1)))

typedef struct page_list {
	uintptr_t head;
	uintptr_t tail;
	unsigned int count;
} page_list_t;

void page_pool_init(uintptr_t base, size_t size, char id);
// uintptr_t page_pool_get(char id);
// uintptr_t page_pool_get_zero(char id);
uintptr_t page_pool_get_pa(char id);
uintptr_t page_pool_get_pa_zero(char id);
// void page_pool_put(uintptr_t page, char id);
// unsigned int page_pool_available(char id);
uintptr_t get_va_pa_offset();
#endif // __ASSEMBLER__