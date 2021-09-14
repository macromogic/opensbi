#ifndef __ASSEMBLER__
#include "drv_handler.h"
#endif
#include "drv_util.h"
#include "drv_syscall.h"
#include "drv_base.h"

void handle_interrupt(uintptr_t *regs, uintptr_t scause, uintptr_t sepc,
		      uintptr_t stval)
{
	em_debug("Start: scause = 0x%lx\n", scause);
	uintptr_t sip, sie;
	switch (scause) {
	case IRQ_S_TIMER:
		em_debug("IRQ_S_TIMER sepc=0x%08x, stval=0x%08x!\n", sepc,
			 stval);
		clear_csr(sip, SIP_STIP);
		break;
	case IRQ_S_SOFT:
		em_debug("IRQ_S_SOFT sepc=0x%08x, stval=0x%08x!\n", sepc,
			 stval);
		clear_csr(sip, SIP_SSIP);
		break;
	case IRQ_S_EXT:
		em_debug("IRQ_S_EXT sepc=0x%08x, stval=0x%08x\n", sepc, stval);
		clear_csr(sip, 1 << IRQ_S_EXT);
		clear_csr(sie, 1 << IRQ_S_EXT);
		break;
	default:
		break;
	}
}

void handle_exception(uintptr_t *regs, uintptr_t scause, uintptr_t sepc,
		      uintptr_t stval)
{
	em_error("scause=%d, sepc=0x%llx, stval=0x%llx!\n", scause, sepc,
		 stval);
	SBI_CALL5(SBI_EXT_EBI, enclave_id, 0, 0, EBI_EXIT);
}

void handle_syscall(uintptr_t *regs, uintptr_t scause, uintptr_t sepc,
		    uintptr_t stval)
{
	em_debug("Start\n");
	em_debug("sepc: 0x%lx\n", sepc);

	uintptr_t sstatus = read_csr(sstatus);
	// sstatus |= SSTATUS_SUM;
	// write_csr(sstatus, sstatus);

	if (scause != CAUSE_USER_ECALL) {
		handle_exception(regs, scause, sepc, stval);
	}

	uintptr_t which = regs[A7_INDEX], arg_0 = regs[A0_INDEX],
		  arg_1 = regs[A1_INDEX], retval = 0;
	em_debug("which: %d\n", which);
	switch (which) {
	case SYS_fstat:
		retval = ebi_fstat(arg_0, arg_1);
		break;
	case SYS_write:
		em_debug("SYS_write\n");
		retval = ebi_write(arg_0, arg_1);
		break;
	case SYS_close:
		retval = ebi_close(arg_0);
		break;
	case SYS_brk:
		em_debug("SYS_brk: arg0 = 0x%lx\n", arg_0);
		retval = ebi_brk(arg_0);
		break;
	case SYS_gettimeofday:
		retval = ebi_gettimeofday((struct timeval *)arg_0,
					  (struct timezone *)arg_1);
		break;
	case SYS_exit:
		// SBI_CALL(EBI_EXIT, enclave_id, arg_0, 0);
		em_debug("SYS_exit\n");
		SBI_CALL5(SBI_EXT_EBI, enclave_id, arg_0, 0, EBI_EXIT);
		break;
	// case EBI_GOTO:
	// 	//TODO SBI_CALL -> SBI_CALL5
	// 	SBI_CALL5(SBI_EXT_EBI, enclave_id, arg_0, 0, EBI_GOTO);
	// 	break;
	default:
		em_error("syscall %d unimplemented!\n", which);
		SBI_CALL5(SBI_EXT_EBI, enclave_id, 0, 0, EBI_EXIT);
		break;
	}
	em_debug("Before writing sepc: sepc = 0x%lx\n", sepc);
	write_csr(sepc, sepc + 4);
	em_debug("After writing sepc: sepc = 0x%lx\n", read_csr(sepc));
	sstatus = sstatus & ~(SSTATUS_SPP | SSTATUS_UIE | SSTATUS_UPIE);
	em_debug("Before write to sstatus\n");
	write_csr(sstatus, sstatus);
	em_debug("After write to sstatus\n");
	em_debug("End\n");
	regs[A0_INDEX] = retval;
}
void unimplemented_exception(uintptr_t *regs, uintptr_t scause, uintptr_t sepc,
			     uintptr_t stval)
{
	em_error("scause=%lx, sepc=%lx, stval=%lx\n", scause, sepc, stval);
}