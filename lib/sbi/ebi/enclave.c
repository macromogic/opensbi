#include <sbi/ebi/enclave.h>
#include <sbi/ebi/memory.h>
#include <sbi/ebi/drv.h>
#include <sbi/ebi/pmp.h>
#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/sbi_string.h>

enclave_context_t enclaves[NUM_ENCLAVE + 1];
int enclave_on_core[NUM_CORES];
spinlock_t enclave_lock, core_lock;

extern char _base_start, _base_end;

#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic push
static inline void dump_csr_context(const enclave_context_t *ectx)
{
	sbi_debug("Dumping enclave context @%p\n", ectx);
	sbi_debug("satp = %lx\n", ectx->ns_satp);
	sbi_debug("medeleg = %lx\n", ectx->ns_medeleg);
	sbi_debug("sie = %lx\n", ectx->ns_sie);
	sbi_debug("stvec = %lx\n", ectx->ns_stvec);
	sbi_debug("sstatus = %lx\n", ectx->ns_sstatus);
	sbi_debug("sscratch = %lx\n", ectx->ns_sscratch);
	sbi_debug("mepc = %lx\n", ectx->ns_mepc);
	sbi_debug("mstatus = %lx\n", ectx->ns_mstatus);
}
#pragma GCC diagnostic pop

static void init_enclave_context(enclave_context_t *ectx)
{
	uintptr_t new_mstatus = csr_read(CSR_MSTATUS) &
				~(MSTATUS_MPP | MSTATUS_SIE | MSTATUS_SPIE);
	new_mstatus |= MSTATUS_MPP & (MSTATUS_MPP >> 1);
	new_mstatus |= MSTATUS_SIE;
	ectx->ns_mstatus       = new_mstatus;
	ectx->ns_sstatus       = csr_read(CSR_SSTATUS) & ~(SSTATUS_SIE);
	ectx->ns_mepc	       = 0x0 + ectx->pa + EUSR_MEM_SIZE;
	ectx->ns_sscratch      = 0;
	ectx->ns_satp	       = 0;
	ectx->ns_sie	       = 0;
	ectx->ns_stvec	       = 0;
	ectx->pt_root_addr     = 0;
	ectx->inverse_map_addr = 0;
	ectx->offset_addr      = 0;
}

static void save_enclave_context(enclave_context_t *ectx, uintptr_t mepc,
				 struct sbi_trap_regs *regs)
{
	ectx->ns_satp	  = csr_read(CSR_SATP);
	ectx->ns_mepc	  = mepc + 4;
	ectx->ns_mstatus  = regs->mstatus;
	ectx->ns_medeleg  = csr_read(CSR_MEDELEG);
	ectx->ns_sie	  = csr_read(CSR_SIE);
	ectx->ns_stvec	  = csr_read(CSR_STVEC);
	ectx->ns_sstatus  = csr_read(CSR_SSTATUS);
	ectx->ns_sscratch = csr_read(CSR_SSCRATCH);
}

static void restore_enclave_context(enclave_context_t *ectx,
				    struct sbi_trap_regs *regs)
{
	csr_write(CSR_SATP, ectx->ns_satp);
	flush_tlb();

	csr_write(CSR_MEDELEG, ectx->ns_medeleg);
	csr_write(CSR_SIE, ectx->ns_sie);
	csr_write(CSR_STVEC, ectx->ns_stvec);
	csr_write(CSR_SSTATUS, ectx->ns_sstatus);
	csr_write(CSR_SSCRATCH, ectx->ns_sscratch);

	regs->mepc    = ectx->ns_mepc - 4;
	regs->mstatus = ectx->ns_mstatus;
}

static void save_umode_context(enclave_context_t *ectx,
			       struct sbi_trap_regs *regs)
{
	sbi_memcpy(ectx->umode_context, regs, INTEGER_CONTEXT_SIZE);
}

static void restore_umode_context(enclave_context_t *ectx,
				  struct sbi_trap_regs *regs)
{
	sbi_memcpy(regs, ectx->umode_context, INTEGER_CONTEXT_SIZE);
}

static void enclave_mem_free(enclave_context_t *ectx)
{
	int eid = ectx->id;

	ectx->offset_addr      = 0;
	ectx->pt_root_addr     = 0;
	ectx->inverse_map_addr = 0;

	sbi_debug("Freeing enclave %d\n", eid);
	free_section_for_enclave(eid);
}

void init_enclaves(void)
{
	init_memory_pool();
	enclaves[0].status = ENC_RUN;
	for (size_t i = 1; i <= NUM_ENCLAVE; ++i)
		enclaves[i].status = ENC_FREE;
	SPIN_LOCK_INIT(&enclave_lock);
	SPIN_LOCK_INIT(&core_lock);
	sbi_debug("enclaves init successfully!");
}

uintptr_t create_enclave(struct sbi_trap_regs *regs, uintptr_t mepc)
{
	uintptr_t payload_addr	= regs->a0;
	size_t payload_size	= regs->a1;
	uintptr_t drv_mask	= regs->a2;
	enclave_context_t *ectx = NULL;
	uintptr_t enclave_id	= 0;
	uintptr_t pa;
	size_t i;
	uintptr_t base_start_addr, base_end_addr;
	uintptr_t enclave_base_addr;
	size_t base_size, drv_size;

	// Check payload size
	sbi_debug("User payload @0x%lx\n", payload_addr);
	if (PAGE_UP(payload_size) > EMEM_SIZE) {
		sbi_error("Payload too large!\n");
		return EBI_ERROR;
	}

	// Initial enclave context, assign an ID to the enclave
	spin_lock(&enclave_lock);
	for (i = 1; i <= NUM_ENCLAVE; ++i) {
		if (enclaves[i].status == ENC_FREE) {
			enclave_id = i;
			break;
		}
	}
	if (enclave_id < 1) {
		sbi_error("No available enclaves!\n");
		spin_unlock(&enclave_lock);
		return EBI_ERROR;
	}
	ectx		       = &enclaves[enclave_id];
	ectx->id	       = enclave_id;
	ectx->pa	       = 0;
	ectx->status	       = ENC_LOAD;
	ectx->pt_root_addr     = 0;
	ectx->offset_addr      = 0;
	ectx->inverse_map_addr = 0;
	sbi_debug("Created enclave with ID=%lx\n", ectx->id);

	// Allocate initial memory
	pa = alloc_section_for_enclave(ectx, EDRV_VA_START);
	if (!pa) {
		sbi_error("Failed to allocate section for enclave #0x%lx\n",
			  ectx->id);
		ectx->status = ENC_FREE;
		spin_unlock(&enclave_lock);
		return EBI_ERROR;
	}
	spin_unlock(&enclave_lock);
	ectx->pa       = pa;
	ectx->mem_size = EMEM_SIZE;

	// Copy base module
	base_start_addr	  = (uintptr_t)&_base_start;
	base_end_addr	  = (uintptr_t)&_base_end;
	base_size	  = PAGE_UP(base_end_addr - base_start_addr);
	enclave_base_addr = ectx->pa + EUSR_MEM_SIZE;
	drv_size	  = 0;
	sbi_memcpy((void *)enclave_base_addr, (void *)base_start_addr,
		   base_size);

	// Copy drivers
	if (drv_mask != 0) {
		enclave_base_addr += base_size;
		drv_size = drv_copy(&enclave_base_addr, drv_mask);
	} else {
		enclave_base_addr = 0;
		drv_size	  = 0;
	}
	sbi_debug(
		"After driver copy: enclave_base_addr=0x%lx, base_start_addr=0x%lx, pa=0x%lx\n",
		enclave_base_addr, base_start_addr, ectx->pa);

	// Copy payload, init context
	memcpy_from_user(ectx->pa, payload_addr, payload_size, mepc);
	init_enclave_context(ectx);
	ectx->enclave_binary_size = payload_size;
	ectx->drv_list		  = enclave_base_addr;
	ectx->user_param	  = enclave_base_addr + drv_size;

	regs->a0 = enclave_id;
	return enclave_id;
}

uintptr_t enter_enclave(struct sbi_trap_regs *regs, uintptr_t mepc)
{
	uintptr_t id		= regs->a0;
	enclave_context_t *ectx = &enclaves[id];
	enclave_context_t *host = &enclaves[0];
	uint32_t hart_id	= current_hartid();
	if (ectx->status != ENC_LOAD || host->status != ENC_RUN) {
		sbi_error("Invalid runtime state!\n");
	}
	sbi_debug("Entering enclave #0x%lx\n", id);

	// Copy user parameters
	memcpy_from_user(ectx->user_param, regs->a2, regs->a1, mepc);

	// Configure PMP, save context
	pmp_switch(ectx);
	save_umode_context(host, regs);
	save_enclave_context(ectx, mepc, regs);
	restore_enclave_context(ectx, regs);
	flush_tlb();

	// Assign the enclave to a core
	spin_lock(&core_lock);
	enclave_on_core[hart_id] = id;
	spin_unlock(&core_lock);

	// Initialize parameters for:
	// init_mem(_, id, mem_start, usr_size, drv_list, argc, argv)
	regs->a5 = regs->a1;		      // a5: argc
	regs->a6 = regs->a2;		      // a6: argv
	regs->a0 = id;			      // a0: (dummy)
	regs->a1 = id;			      // a1: mem_start
	regs->a2 = ectx->pa;		      // a2: mem_start
	regs->a3 = ectx->enclave_binary_size; // a3: usr_size
	regs->a4 = ectx->drv_list;	      // a4: drv_list
	// argc and argv may be unused

	// Switch runtime status
	spin_lock(&enclave_lock);
	host->status = ENC_IDLE;
	ectx->status = ENC_RUN;
	spin_unlock(&enclave_lock);
	return id;
}

uintptr_t exit_enclave(struct sbi_trap_regs *regs)
{
	uintptr_t id		= regs->a0;
	uintptr_t ret_val	= regs->a1;
	uint32_t hart_id	= current_hartid();
	enclave_context_t *ectx = &enclaves[id];
	enclave_context_t *host = &enclaves[0];
	if (ectx->status == ENC_RUN || host->status == ENC_IDLE) {
		sbi_error("Invalid runtime state!\n");
		return EBI_ERROR;
	}
	sbi_debug("Exiting encalve #0x%lx\n", id);

	// Free encalve memory, clean registers
	enclave_mem_free(ectx);
	spin_lock(&core_lock);
	enclave_on_core[hart_id] = 0;
	spin_unlock(&core_lock);
	pmp_switch(NULL);
	restore_umode_context(host, regs);
	restore_enclave_context(host, regs);
	regs->mepc += 4;

	// Set return value, switch runtime status
	regs->a0 = ret_val;
	spin_lock(&enclave_lock);
	ectx->status = ENC_FREE;
	ectx->status = ENC_RUN;
	spin_unlock(&enclave_lock);
	return EBI_OK;
}

uintptr_t pause_enclave(uintptr_t eid, uintptr_t *regs, uintptr_t mepc)
{
	// TODO
	return 0;
}

uintptr_t resume_enclave(uintptr_t eid, uintptr_t *regs)
{
	// TODO
	return 0;
}

enclave_context_t *eid_to_context(uintptr_t eid)
{
	return &enclaves[eid];
}