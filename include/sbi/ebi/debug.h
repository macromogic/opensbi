#ifndef EBI_DEBUG_H
#define EBI_DEBUG_H

#include <sbi/sbi_trap.h>

void enclave_debug(struct sbi_trap_regs *regs);

#endif