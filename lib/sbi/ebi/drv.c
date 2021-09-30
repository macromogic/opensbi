#include <sbi/ebi/drv.h>
#include <sbi/ebi/enclave.h>
#include <sbi/ebi/memory.h>
#include <sbi/sbi_string.h>
#include <sbi/riscv_asm.h>
#include <sbi/riscv_locks.h>

extern char _console_start, _console_end;
drv_addr_t __drv_addr_list[MAX_DRV] = { { .drv_start =
						  (uintptr_t)&_console_start,
					  .drv_end  = (uintptr_t)&_console_end,
					  .using_by = -1 } };
drv_addr_t *drv_addr_list = __drv_addr_list; // Tricky! Do not change this!
spinlock_t drv_lock[MAX_DRV];

void drv_fetch(uintptr_t drv_to_fetch)
{
	spin_lock(&drv_lock[drv_to_fetch]);
}

void drv_release(uintptr_t drv_to_release)
{
	spin_unlock(&drv_lock[drv_to_release]);
}

uintptr_t copy_drv_with_list(uintptr_t *dst_addr, uintptr_t drv_mask)
{
	// Default initialization implicitly calls `memset'
	drv_addr_t local_addr_list[MAX_DRV];
	int cnt = 0;
	int i;
	uintptr_t drv_start, drv_size;
	for (i = 0; i < MAX_DRV; ++i) {
		if ((drv_mask & (1 << i)) && drv_addr_list[i].drv_start) {
			drv_start = drv_addr_list[i].drv_start;
			drv_size  = drv_addr_list[i].drv_end -
				   drv_addr_list[i].drv_start;
			local_addr_list[cnt].drv_start = *dst_addr;
			local_addr_list[cnt].drv_end   = *dst_addr + drv_size;
			++cnt;
			sbi_memcpy((void *)(*dst_addr), (void *)drv_start,
				   drv_size);
			*dst_addr += drv_size;
		}
	}
	if (i < MAX_DRV) {
		local_addr_list[i].drv_start = 0; // Mark the end of the list
		local_addr_list[i].drv_end   = 0;
		local_addr_list[i].using_by  = 0;
	}
	sbi_memcpy((void *)*dst_addr, (void *)local_addr_list,
		   sizeof(local_addr_list));
	return sizeof(local_addr_list);
}

void inform_peripheral(struct sbi_trap_regs *regs)
{
	uint32_t hartid		= current_hartid();
	enclave_context_t *ectx = &enclaves[enclave_on_core[hartid]];
	uintptr_t pa		= regs->a0;
	uintptr_t va		= regs->a1;
	uintptr_t size		= regs->a2;

	peri_addr_t *peri  = &(ectx->peri_list[ectx->peri_cnt]);
	peri->reg_pa_start = pa;
	peri->reg_va_start = va;
	peri->reg_size	   = size;
	sbi_debug("Peripheral #%d: pa=0x%lx, va=0x%lx, size=0x%lx\n",
		  ectx->peri_cnt, pa, va, size);
	ectx->peri_cnt++;
}
