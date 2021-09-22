#include <sbi/ebi/debug.h>
#include <sbi/ebi/enclave.h>
#include <sbi/ebi/memory.h>
#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/sbi_string.h>
#include <sbi/ebi/monitor.h>

void enclave_debug(struct sbi_trap_regs *regs)
{
	uintptr_t debug_id = regs->a0;
	uintptr_t hartid = csr_read(CSR_MHARTID);
	sbi_printf("[enclave_debug] begin ------------------------------------------------\n");

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

	default:
		break;
	}

	sbi_printf("[enclave_debug] end   ------------------------------------------------\n");
}