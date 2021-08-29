#ifndef EBI_DRV_H
#define EBI_DRV_H

#include <sbi/ebi/util.h>

#define MAX_DRV 64
#define CMD_QUERY_INFO -1

typedef uintptr_t (*cmd_handler)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);

typedef struct {
	uintptr_t reg_addr;
	uintptr_t reg_size;
} drv_ctrl_t;

typedef struct {
	uintptr_t drv_start;
	uintptr_t drv_end;
	int using_by; // May be removed
} drv_addr_t;

extern drv_addr_t drv_addr_list[MAX_DRV];
uintptr_t drv_copy(uintptr_t *dst_addr, uintptr_t drv_mask);
void inform_peripheral(struct sbi_trap_regs *regs);

#endif // EBI_DRV_H
