#include "drv_syscall.h"
#include "drv_util.h"
#ifndef __ASSEMBLER__
#include "drv_mem.h"
#endif
#include "mm/drv_page_pool.h"
#include "mm/page_table.h"
#include "drv_base.h"
#include "drv_list.h"
#include "../drv_console/drv_console.h"

extern uintptr_t prog_brk;
// extern uintptr_t pt_root;
extern drv_addr_t *drv_addr_list;
int ebi_fstat(uintptr_t fd, uintptr_t sstat)
{
	struct stat *stat = (struct stat *)sstat;
	/* now only support stdio */
	if (fd > 4 || fd < 0) {
		return -1;
	}
	stat->st_dev	 = 26;
	stat->st_ino	 = 6;
	stat->st_nlink	 = 1;
	stat->st_mode	 = S_IWUSR | S_IRUSR | S_IRGRP;
	stat->st_uid	 = 1000;
	stat->st_gid	 = 5;
	stat->st_rdev	 = 34819;
	stat->st_size	 = 0;
	stat->st_blksize = 1024;
	stat->st_blocks	 = 0;
	return 0;
}

int ebi_brk(uintptr_t addr)
{
	uintptr_t n_pages, pa;
	if (addr == 0)
		return prog_brk;
#ifdef EMODULE_GLOBAL_DEBUG
	printd("####### brk start, prog_brk: 0x%lx########\n", prog_brk);
	printd("addr: 0x%lx\n", addr);
#endif
	if (addr > PAGE_UP(prog_brk)) { // currently freeing does not work
#ifdef EMODULE_GLOBAL_DEBUG
		printd("ebi_brk cp 1\n");
#endif
		n_pages = PAGE_UP(addr - prog_brk) >> EPAGE_SHIFT;
#ifdef EMODULE_GLOBAL_DEBUG
		printd("ebi_brk cp 2 n_pages = 0x%lx\n", n_pages);
#endif
		alloc_page(NULL, PAGE_UP(prog_brk), n_pages,
			   PTE_U | PTE_R | PTE_W, USR);
#ifdef EMODULE_GLOBAL_DEBUG
		printd("ebi_brk cp 3\n");
#endif
	}
	prog_brk = addr;
#ifdef EMODULE_GLOBAL_DEBUG
	printd("####### brk end########\n");
#endif
	flush_tlb();
	return addr;
}

int ebi_write(uintptr_t fd, uintptr_t content)
{
	/* stdout */
	drv_fetch(DRV_CONSOLE);
	cmd_handler console_handler =
		(cmd_handler)drv_addr_list[DRV_CONSOLE].drv_start;
	if (fd == 1) {
		char *str = (char *)content;
		while (*str) {
			console_handler(CONSOLE_CMD_PUT, *str, 0, 0);
			str++;
		}
	}
	drv_release(DRV_CONSOLE);
	return 0;
}

int ebi_close(uintptr_t fd)
{
	return 0;
}

int ebi_gettimeofday(struct timeval *tv, struct timezone *tz)
{
	// if (!drv_list[DRV_RTC]) return ERR_DRV_NOT_FND;
	// if (!tv) return EFAULT;

	// cmd_handler rtc_handler = (cmd_handler)drv_addr_list[DRV_RTC].drv_start;

	// // uintptr_t time = rtc_handler(RTC_CMD_GET_TIME, 0, 0, 0);
	// tv->tv_sec = time / 1000000000;
	// tv->tv_usec = (time % 1000000000) / 1000; //  microsecond (μs)

	// printd("***** gettimeofday: second: %ld, microsecond: %ld *****\n", tv->tv_sec, tv->tv_usec);

	return 0;
}