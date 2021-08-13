/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#include <sbi/sbi_console.h>
#include <sbi/sbi_ecall.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_trap.h>
#include <sbi/sbi_console.h>
#include <sbi/riscv_encoding.h>
#include <sbi/riscv_asm.h>
#include <sbi/sbi_ecall_ebi_enclave.h>

u16 sbi_ecall_version_major(void)
{
	return SBI_ECALL_VERSION_MAJOR;
}

u16 sbi_ecall_version_minor(void)
{
	return SBI_ECALL_VERSION_MINOR;
}

static unsigned long ecall_impid = SBI_OPENSBI_IMPID;

unsigned long sbi_ecall_get_impid(void)
{
	return ecall_impid;
}

void sbi_ecall_set_impid(unsigned long impid)
{
	ecall_impid = impid;
}

static SBI_LIST_HEAD(ecall_exts_list);

struct sbi_ecall_extension *sbi_ecall_find_extension(unsigned long extid)
{
	struct sbi_ecall_extension *t, *ret = NULL;

	sbi_list_for_each_entry(t, &ecall_exts_list, head)
	{
		if (t->extid_start <= extid && extid <= t->extid_end) {
			ret = t;
			break;
		}
	}

	return ret;
}

int sbi_ecall_register_extension(struct sbi_ecall_extension *ext)
{
	struct sbi_ecall_extension *t;

	if (!ext || (ext->extid_end < ext->extid_start) || !ext->handle)
		return SBI_EINVAL;

	sbi_list_for_each_entry(t, &ecall_exts_list, head)
	{
		unsigned long start = t->extid_start;
		unsigned long end   = t->extid_end;
		if (end < ext->extid_start || ext->extid_end < start)
			/* no overlap */;
		else
			return SBI_EINVAL;
	}

	SBI_INIT_LIST_HEAD(&ext->head);
	sbi_list_add_tail(&ext->head, &ecall_exts_list);

	return 0;
}

void sbi_ecall_unregister_extension(struct sbi_ecall_extension *ext)
{
	bool found = FALSE;
	struct sbi_ecall_extension *t;

	if (!ext)
		return;

	sbi_list_for_each_entry(t, &ecall_exts_list, head)
	{
		if (t == ext) {
			found = TRUE;
			break;
		}
	}

	if (found)
		sbi_list_del_init(&ext->head);
}

int sbi_ecall_handler(struct sbi_trap_regs *regs)
{
	int ret = 0;
	struct sbi_ecall_extension *ext;
	unsigned long extension_id = regs->a7;
	unsigned long func_id	   = regs->a6;
	struct sbi_trap_info trap  = { 0 };
	unsigned long out_val	   = 0;
	bool is_0_1_spec	   = 0;

	if (extension_id == SBI_EXT_EBI) {
		sbi_printf(
			"[sbi_ecall_handler] Calling EBI with function ID=%lu\n",
			func_id);
	}

	ulong mcause	= csr_read(CSR_MCAUSE);
	ulong mtval	= csr_read(CSR_MTVAL);
	ulong mtval2	= 0;
	ulong mtinst	= 0;
	ulong prev_mode = (regs->mstatus & MSTATUS_MPP) >> MSTATUS_MPP_SHIFT;
	if (prev_mode == 0 && extension_id != SBI_EXT_EBI) {
		// The U-mode ecall is a system call if a7 is not SBI_EXT_EBI
		trap.epc   = regs->mepc;
		trap.cause = mcause;
		trap.tval  = mtval;
		trap.tval2 = mtval2;
		trap.tinst = mtinst;
		sbi_trap_redirect(regs, &trap);
		return 0;
	}

	ext = sbi_ecall_find_extension(extension_id);
	if (ext && ext->handle) {
		ret = ext->handle(extension_id, func_id, regs, &out_val, &trap);
		if (extension_id >= SBI_EXT_0_1_SET_TIMER &&
		    extension_id <= SBI_EXT_0_1_SHUTDOWN)
			is_0_1_spec = 1;
	} else {
		ret = SBI_ENOTSUPP;
	}

	if (extension_id == SBI_EXT_EBI) {
		sbi_printf("[sbi_ecall_handler] EBI ret = %d\n", ret);
	}

	if (ret == SBI_ETRAP) {
		trap.epc = regs->mepc;
		sbi_trap_redirect(regs, &trap);
	} else {
		if (ret < SBI_LAST_ERR) {
			sbi_printf("%s: Invalid error %d for ext=0x%lx "
				   "func=0x%lx\n",
				   __func__, ret, extension_id, func_id);
			ret = SBI_ERR_FAILED;
		}

		/*
		 * This function should return non-zero value only in case of
		 * fatal error. However, there is no good way to distinguish
		 * between a fatal and non-fatal errors yet. That's why we treat
		 * every return value except ETRAP as non-fatal and just return
		 * accordingly for now. Once fatal errors are defined, that
		 * case should be handled differently.
		 */
		if (extension_id == SBI_EXT_EBI)
			sbi_printf("[sbi_ecall_handler] Ding\n");
		regs->mepc += 4;
		regs->a0 = ret;
		if (!is_0_1_spec)
			regs->a1 = out_val;
		if (extension_id == SBI_EXT_EBI)
			sbi_printf("[sbi_ecall_handler] Dong\n");
	}

	if (extension_id == SBI_EXT_EBI) {
		sbi_printf("[sbi_ecall_handler] Ret\n");
		sbi_printf("[sbi_ecall_handler] regs->a1 = %lx\n", regs->a1);
		sbi_printf("[sbi_ecall_handler] regs->a2 = %lx\n", regs->a2);
		sbi_printf("[sbi_ecall_handler] regs->a3 = %lx\n", regs->a3);
		sbi_printf("[sbi_ecall_handler] regs->a4 = %lx\n", regs->a4);
		sbi_printf("[sbi_ecall_handler] regs->a5 = %lx\n", regs->a5);
		sbi_printf("[sbi_ecall_handler] regs->a6 = %lx\n", regs->a6);
		sbi_printf("[sbi_ecall_handler] mepc=%lx, mstatus=%lx\n",
			   csr_read(CSR_MEPC), csr_read(CSR_MSTATUS));
	}

	return 0;
}

int sbi_ecall_init(void)
{
	int ret;

	/* The order of below registrations is performance optimized */
	ret = sbi_ecall_register_extension(&ecall_time);
	if (ret)
		return ret;
	ret = sbi_ecall_register_extension(&ecall_rfence);
	if (ret)
		return ret;
	ret = sbi_ecall_register_extension(&ecall_ipi);
	if (ret)
		return ret;
	ret = sbi_ecall_register_extension(&ecall_base);
	if (ret)
		return ret;
	ret = sbi_ecall_register_extension(&ecall_hsm);
	if (ret)
		return ret;
	ret = sbi_ecall_register_extension(&ecall_srst);
	if (ret)
		return ret;
	ret = sbi_ecall_register_extension(&ecall_legacy);
	if (ret)
		return ret;
	ret = sbi_ecall_register_extension(&ecall_vendor);
	if (ret)
		return ret;
	ret = sbi_ecall_register_extension(&ecall_ebi);
	init_enclaves();
	sbi_printf("############### init ecall_ebi successfully\n");
	sbi_printf("ecall_ebi: %p\n", ecall_ebi.handle);
	if (ret)
		return ret;

	return 0;
}
