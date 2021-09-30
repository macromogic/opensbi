#include <sbi/sbi_ecall.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_trap.h>
#include <sbi/sbi_version.h>
#include <sbi/riscv_asm.h>
#include <sbi/sbi_console.h>
#include <sbi/ebi/enclave.h>
#include <sbi/ebi/memory.h>

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

#ifdef EBI_DEBUG
	uintptr_t linux_satp = csr_read(CSR_SATP); //debug
#endif
	uintptr_t va, pa;

	switch (funcid) {
	case EBI_CREATE:
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
		return ret;

	case EBI_ENTER:
		sbi_debug("enter\n");
		enter_enclave(regs, mepc);
		sbi_debug("back from enter_enclave\n");
		sbi_debug("id = %lx, into->pa: 0x%lx\n", regs->a1, regs->a2);
		return ret;

	case EBI_EXIT:
		sbi_debug("enclave %lx exit\n", regs->a0);
		exit_enclave(regs);
		return ret;

	case EBI_PERI_INFORM:
		inform_peripheral(regs);
		return ret;

	case EBI_MEM_ALLOC:
		sbi_debug("SBI_EXT_EBI_MEM_ALLOC\n");
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
		return ret;

	case EBI_MAP_REGISTER:
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
		return ret;

	case EBI_FLUSH_DCACHE:
		// asm volatile(".word 0xFC000073"
		// 	     :
		// 	     :
		// 	     : "memory"); // cflush.d.l1 zero
		// TODO Clean L2?
		return ret;

	case EBI_DISCARD_DCACHE:
		asm volatile(".word 0xFC200073"
			     :
			     :
			     : "memory"); // cdiscard.d.l1 zero
		// TODO Clean L2?
		return ret;
	}

	return ret;
}

struct sbi_ecall_extension ecall_ebi = {
	.extid_start = SBI_EXT_EBI,
	.extid_end   = SBI_EXT_EBI,
	.handle	     = sbi_ecall_ebi_handler,
};
