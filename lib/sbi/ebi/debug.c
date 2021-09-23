#include <sbi/ebi/debug.h>
#include <sbi/ebi/enclave.h>
#include <sbi/ebi/memory.h>
#include <sbi/ebi/memutil.h>
#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/sbi_string.h>
#include <sbi/ebi/monitor.h>

void enclave_debug(struct sbi_trap_regs *regs)
{
	uintptr_t debug_id = regs->a0;
	uintptr_t hartid = csr_read(CSR_MHARTID);
	sbi_debug("[enclave_debug] begin debug_id = %lx----------------------------------\n", debug_id);

	switch (debug_id) {
	case 0:
		dump_enclave_status();
		break;

	case 1:
		sbi_printf("[ INFO ] hartid = 0x%lx\n", hartid);
		break;
	
	case 2:
		dump_section_ownership();
		break;

	case 3:
		regs->a0 = enclave_num();
		break;

	case 4:
		regs->a0 = compacted;
		break;
	
	case 5:
		; uintptr_t id = regs->a1;
		regs->a0 = check_alive(id);
		break;

	case 6:
		; uintptr_t addr = regs->a1;
		uintptr_t len = regs->a2;
		debug_memdump(addr, len);
		break;

	default:
		break;
	}

	sbi_debug("[enclave_debug] end   ---------------------------------------------\n");
}