#include <sbi/sbi_ecall.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_trap.h>
#include <sbi/sbi_version.h>
#include <sbi/riscv_asm.h>
#include <sbi/sbi_console.h>
#include <sbi/ebi/enclave.h>
#include <sbi/ebi/memory.h>
#include <sbi/ebi/debug.h>
#include <sbi/riscv_locks.h>

spinlock_t overall_lock;

extern char _base_start, _base_end;
extern char _enclave_start, _enclave_end;

static int hartid_to_eid(int hartid)
{
	return enclave_on_core[hartid];
}

static int sbi_ecall_ebi_handler(unsigned long extid, unsigned long funcid,
				 struct sbi_trap_regs *regs,
				 unsigned long *out_val,
				 struct sbi_trap_info *out_trap)
{
	int ret			= 0;
	ulong core		= csr_read(CSR_MHARTID);
	ulong mepc		= csr_read(CSR_MEPC);
	int eid			= hartid_to_eid(core);
	enclave_context_t *ectx = &enclaves[eid];
	uintptr_t va, pa;

#ifdef EBI_DEBUG
	uintptr_t linux_satp = csr_read(CSR_SATP); //debug
#endif
	spin_lock(&overall_lock);
	switch (funcid) {
	case SBI_EXT_EBI_CREATE:
		sbi_debug("linux satp = 0x%lx\n", linux_satp);
		sbi_debug("SBI_EXT_EBI_CREATE\n");
		sbi_debug(
			"extid = %lu, funcid = 0x%lx, args[0] = 0x%lx, args[1] = 0x%lx, core = %lu\n",
			extid, funcid, regs->a0, regs->a1, core);
		sbi_debug("_base_start @ %p, _base_end @ %p\n", &_base_start,
			  &_base_end);
		sbi_debug("_enclave_start @ %p, _enclave_end @ %p\n",
			  &_enclave_start, &_enclave_end);
		ret = create_enclave(regs, mepc);
		sbi_debug("after create_enclave\n");
		sbi_debug("regs->a1 = %lx\n", regs->a1);
		sbi_debug("regs->a2 = %lx\n", regs->a2);
		sbi_debug("regs->a3 = %lx\n", regs->a3);
		sbi_debug("regs->a4 = %lx\n", regs->a4);
		sbi_debug("regs->a5 = %lx\n", regs->a5);
		sbi_debug("regs->a6 = %lx\n", regs->a6);
		sbi_debug("mepc=%lx, mstatus=%lx\n", csr_read(CSR_MEPC),
			  csr_read(CSR_MSTATUS));
		break;

	case SBI_EXT_EBI_ENTER:
		sbi_debug("enter\n");
		enter_enclave(regs, mepc);
		sbi_debug("back from enter_enclave\n");
		sbi_debug("id = %lx, into->pa: 0x%lx\n", regs->a1, regs->a2);
		break;

	case SBI_EXT_EBI_EXIT:
		sbi_debug("enclave %lx exit\n", regs->a0);
		exit_enclave(regs);
		break;

	case SBI_EXT_EBI_SUSPEND:
		sbi_debug("suspend enclave %lx\n", regs->a0);
		suspend_enclave(eid, regs, mepc);
		resume_enclave(0, regs);
		break;
	
	case SBI_EXT_EBI_RESUME:
		sbi_debug("resume enclave %lx\n", regs->a0);
		if (eid != 0) {
			sbi_error("should call resume from Linux\n");
			break;
		}
		suspend_enclave(0, regs, mepc);
		resume_enclave(regs->a0, regs);
		break;

	case SBI_EXT_EBI_PERI_INFORM:
		inform_peripheral(regs);
		break;

	case SBI_EXT_EBI_FETCH:
		sbi_debug("SBI_EXT_EBI_FETCH\n");
		uintptr_t drv_to_fetch = regs->a0;
		drv_fetch(drv_to_fetch);
		break;
	
	case SBI_EXT_EBI_RELEASE:
		sbi_debug("SBI_EXT_EBI_RELEASE\n");
		uintptr_t drv_to_release = regs->a1;
		drv_release(drv_to_release);
		break;

	case SBI_EXT_EBI_MEM_ALLOC:
		sbi_debug("SBI_EXT_EBI_MEM_ALLOC\n");
		;
		va = regs->a0;
		// pa should be passed to enclave by regs
		pa = alloc_section_for_enclave(ectx, va);
		if (pa) {
			regs->a1 = pa;
			regs->a2 = SECTION_SIZE;
		} else {
			sbi_error("allocation failed\n");
			exit_enclave(regs);
		}
		break;

	case SBI_EXT_EBI_MAP_REGISTER:
		sbi_debug("SBI_EXT_EBI_MAP_REGISTER\n");
		sbi_debug("&pt_root = 0x%lx\n", regs->a0);
		sbi_debug("&inv_map = 0x%lx\n", regs->a1);
		sbi_debug("&ENC_VA_PA_OFFSET = 0x%lx\n", regs->a2);
		if (!(regs->a0 && regs->a1 && regs->a2)) {
			sbi_error("Invalid ecall, check input\n");
			ret = SBI_ERR_INVALID_PARAM;
			return ret;
		}
		ectx->pt_root_addr     = regs->a0;
		ectx->inverse_map_addr = regs->a1;
		ectx->offset_addr      = regs->a2;
		break;

	case SBI_EXT_EBI_FLUSH_DCACHE:
		asm volatile(".long 0xFC000073"); // cflush.d.l1 zero
		// TODO Clean L2?
		break;

	case SBI_EXT_EBI_DISCARD_DCACHE:
		asm volatile(".long 0xFC200073"); // cdiscard.d.l1 zero
		// TODO Clean L2?
		break;

	case SBI_EXT_EBI_DEBUG:
		enclave_debug(regs);
		break;
	}
	spin_unlock(&overall_lock);

	return ret;
}

struct sbi_ecall_extension ecall_ebi = {
	.extid_start = SBI_EXT_EBI,
	.extid_end   = SBI_EXT_EBI,
	.handle	     = sbi_ecall_ebi_handler,
};
