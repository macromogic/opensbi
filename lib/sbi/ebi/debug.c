#include <sbi/ebi/debug.h>
#include <sbi/ebi/enclave.h>
#include <sbi/ebi/memory.h>
#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/sbi_string.h>

void enclave_debug(struct sbi_trap_regs *regs)
{
	sbi_printf("[enclave_debug] ------------------------\n");
}