#ifndef ENCLAVE_H
#define ENCLAVE_H

#include <sbi/riscv_encoding.h>
#include <sbi/ebi/util.h>

#define SBI_EXT_EBI 0x19260817

#ifndef __ASSEMBLER__
#include <stdint.h>

#define MAX_DRV 64
#define QUERY_INFO -1

#define read_csr(reg)                                         \
	({                                                    \
		unsigned long __tmp;                          \
		asm volatile("csrr %0, " #reg : "=r"(__tmp)); \
		__tmp;                                        \
	})

#define write_csr(reg, val) ({ asm volatile("csrw " #reg ", %0" ::"rK"(val)); })

// See LICENSE for license details.

#endif // __ASSEMBLER__

#ifndef RISCV_CSR_ENCODING_H
#define RISCV_CSR_ENCODING_H

#define MSTATUS_UIE 0x00000001
#define MSTATUS_UPIE 0x00000010
#define SSTATUS_UIE 0x00000001
#define SSTATUS_UPIE 0x00000010

#define DCSR_XDEBUGVER (3U << 30)
#define DCSR_NDRESET (1 << 29)
#define DCSR_FULLRESET (1 << 28)
#define DCSR_EBREAKM (1 << 15)
#define DCSR_EBREAKH (1 << 14)
#define DCSR_EBREAKS (1 << 13)
#define DCSR_EBREAKU (1 << 12)
#define DCSR_STOPCYCLE (1 << 10)
#define DCSR_STOPTIME (1 << 9)
#define DCSR_CAUSE (7 << 6)
#define DCSR_DEBUGINT (1 << 5)
#define DCSR_HALT (1 << 3)
#define DCSR_STEP (1 << 2)
#define DCSR_PRV (3 << 0)

#define DCSR_CAUSE_NONE 0
#define DCSR_CAUSE_SWBP 1
#define DCSR_CAUSE_HWBP 2
#define DCSR_CAUSE_DEBUGINT 3
#define DCSR_CAUSE_STEP 4
#define DCSR_CAUSE_HALT 5

#define MCONTROL_TYPE(xlen) (0xfULL << ((xlen)-4))
#define MCONTROL_DMODE(xlen) (1ULL << ((xlen)-5))
#define MCONTROL_MASKMAX(xlen) (0x3fULL << ((xlen)-11))

#define MCONTROL_SELECT (1 << 19)
#define MCONTROL_TIMING (1 << 18)
#define MCONTROL_ACTION (0x3f << 12)
#define MCONTROL_CHAIN (1 << 11)
#define MCONTROL_MATCH (0xf << 7)
#define MCONTROL_M (1 << 6)
#define MCONTROL_H (1 << 5)
#define MCONTROL_S (1 << 4)
#define MCONTROL_U (1 << 3)
#define MCONTROL_EXECUTE (1 << 2)
#define MCONTROL_STORE (1 << 1)
#define MCONTROL_LOAD (1 << 0)

#define MCONTROL_TYPE_NONE 0
#define MCONTROL_TYPE_MATCH 2

#define MCONTROL_ACTION_DEBUG_EXCEPTION 0
#define MCONTROL_ACTION_DEBUG_MODE 1
#define MCONTROL_ACTION_TRACE_START 2
#define MCONTROL_ACTION_TRACE_STOP 3
#define MCONTROL_ACTION_TRACE_EMIT 4

#define MCONTROL_MATCH_EQUAL 0
#define MCONTROL_MATCH_NAPOT 1
#define MCONTROL_MATCH_GE 2
#define MCONTROL_MATCH_LT 3
#define MCONTROL_MATCH_MASK_LOW 4
#define MCONTROL_MATCH_MASK_HIGH 5

#define IRQ_S_SOFT 1
#define IRQ_H_SOFT 2
#define IRQ_M_SOFT 3
#define IRQ_S_TIMER 5
#define IRQ_H_TIMER 6
#define IRQ_M_TIMER 7
#define IRQ_S_EXT 9
#define IRQ_H_EXT 10
#define IRQ_M_EXT 11
#define IRQ_COP 12
#define IRQ_HOST 13

#define DEFAULT_RSTVEC 0x00001000
#define CLINT_BASE 0x02000000
#define CLINT_SIZE 0x000c0000
#define DRAM_BASE 0x40000000

// page table entry (PTE) fields
#define PTE_V 0x001    // Valid
#define PTE_R 0x002    // Read
#define PTE_W 0x004    // Write
#define PTE_X 0x008    // Execute
#define PTE_U 0x010    // User
#define PTE_G 0x020    // Global
#define PTE_A 0x040    // Accessed
#define PTE_D 0x080    // Dirty
#define PTE_SOFT 0x300 // Reserved for Software

#define PTE_PPN_SHIFT 10

#define PTE_TABLE(PTE) (((PTE) & (PTE_V | PTE_R | PTE_W | PTE_X)) == PTE_V)

#ifdef __riscv

#if __riscv_xlen == 64
#define MSTATUS_SD MSTATUS64_SD
#define SSTATUS_SD SSTATUS64_SD
#define RISCV_PGLEVEL_BITS 9
#define SATP_MODE SATP64_MODE
#else
#define MSTATUS_SD MSTATUS32_SD
#define SSTATUS_SD SSTATUS32_SD
#define RISCV_PGLEVEL_BITS 10
#define SATP_MODE SATP32_MODE
#endif // __riscv_xlen

#define RISCV_PGSHIFT 12
#define RISCV_PGSIZE (1 << RISCV_PGSHIFT)

#ifndef __ASSEMBLER__

#ifdef __GNUC__

#define read_csr(reg)                                         \
	({                                                    \
		unsigned long __tmp;                          \
		asm volatile("csrr %0, " #reg : "=r"(__tmp)); \
		__tmp;                                        \
	})

#define write_csr(reg, val) ({ asm volatile("csrw " #reg ", %0" ::"rK"(val)); })

#define swap_csr(reg, val)                            \
	({                                            \
		unsigned long __tmp;                  \
		asm volatile("csrrw %0, " #reg ", %1" \
			     : "=r"(__tmp)            \
			     : "rK"(val));            \
		__tmp;                                \
	})

#define set_csr(reg, bit)                             \
	({                                            \
		unsigned long __tmp;                  \
		asm volatile("csrrs %0, " #reg ", %1" \
			     : "=r"(__tmp)            \
			     : "rK"(bit));            \
		__tmp;                                \
	})

#define clear_csr(reg, bit)                           \
	({                                            \
		unsigned long __tmp;                  \
		asm volatile("csrrc %0, " #reg ", %1" \
			     : "=r"(__tmp)            \
			     : "rK"(bit));            \
		__tmp;                                \
	})

#define rdtime() read_csr(time)
#define rdcycle() read_csr(cycle)
#define rdinstret() read_csr(instret)

#endif // __GNUC__

#endif // __ASSEMBLER__

#endif // __riscv

#endif // RISCV_CSR_ENCODING_H

#endif // ENCLAVE_H