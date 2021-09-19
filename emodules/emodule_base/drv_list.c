#include "drv_list.h"
#include "mm/drv_page_pool.h"
#include "mm/page_table.h"
#include "../drv_console/drv_console.h"

drv_ctrl_t *init_console_driver()
{
	uintptr_t drv_console_start, drv_console_end, console_drv_size,
		console_va;
	size_t n_console_drv_pages;
	cmd_handler console_handler;
	drv_ctrl_t *console_ctrl;

	em_debug("Start\n");

	em_debug("driver list @0x%lx\n", drv_addr_list);
	em_debug("console handler @0x%lx\n", &drv_addr_list[DRV_CONSOLE]);
	em_debug("console handler @0x%lx\n",
		 drv_addr_list[DRV_CONSOLE].drv_start);
	drv_console_start = drv_addr_list[DRV_CONSOLE].drv_start;
	console_handler	  = (cmd_handler)drv_console_start;
	console_ctrl	  = (drv_ctrl_t *)console_handler(QUERY_INFO, 0, 0, 0);

	em_debug("Before ioremap\n");
	console_va = ioremap(NULL, console_ctrl->reg_addr, 1024);
	em_debug("After ioremap\n");
	uintptr_t i;
	uint32_t val;
	em_debug(">>> BEFORE <<<\n");
	for (i = 0; i < 0x1c; i += 4) {
		val = readl((void *)(console_va + i));
		em_debug("Reg @0x%x: %x\n", i, val);
	}
	SBI_CALL5(SBI_EXT_EBI, console_ctrl->reg_addr, console_va,
		  PAGE_UP(console_ctrl->reg_size), EBI_PERI_INFORM);

	em_debug("console_va: 0x%x\n, entry: 0x%lx\n", console_va,
		 get_pa(console_va));
	em_debug("\033[0;36mconsole_handler at %p\n\033[0m", console_handler);
	console_handler(CONSOLE_CMD_INIT, console_va, 0, 0);

	em_debug("Console driver init successfully\n");
	em_debug(">>> AFTER <<<\n");
	for (i = 0; i < 0x1c; i += 4) {
		val = readl((void *)(console_va + i));
		em_debug("Reg @0x%x: %x\n", i, val);
	}
	// console_handler(CONSOLE_CMD_PUT, 'c', 0, 0
	// console_handler(CONSOLE_CMD_PUT, 'o', 0, 0);
	// console_handler(CONSOLE_CMD_PUT, 'm', 0, 0);
	// console_handler(CONSOLE_CMD_PUT, 'p', 0, 0);
	// console_handler(CONSOLE_CMD_PUT, 'a', 0, 0);
	// console_handler(CONSOLE_CMD_PUT, 's', 0, 0);
	// console_handler(CONSOLE_CMD_PUT, 's', 0, 0);
	// console_handler(CONSOLE_CMD_PUT, '\n', 0, 0);

	return console_ctrl;
}

// drv_ctrl_t* init_rtc_driver() {
//     printd("init rtc driver\n");

//     uintptr_t drv_rtc_start = drv_addr_list[DRV_RTC].drv_start,
//               drv_rtc_end = drv_addr_list[DRV_RTC].drv_end;
//     printd("rtc driver: %x ... %x\n", drv_rtc_start, drv_rtc_end);

//     uintptr_t drv_rtc_size = drv_rtc_end - drv_rtc_start;
//     size_t n_drv_rtc_pages = (PAGE_UP(drv_rtc_size)) >> EPAGE_SHIFT;
//     map_page((pte *)pt_root, drv_rtc_start, drv_rtc_start - ENC_VA_PA_OFFSET, n_drv_rtc_pages, PTE_V | PTE_X | PTE_W | PTE_R);

//     cmd_handler rtc_handler = (cmd_handler)drv_rtc_start;

//     drv_ctrl_t* rtc_ctrl = (drv_ctrl_t*)rtc_handler(QUERY_INFO, 0, 0, 0);
//     printd("RTC reg phy addr: %x\n", rtc_ctrl->reg_addr);
//     uintptr_t rtc_reg_va = ioremap((pte *)pt_root, rtc_ctrl->reg_addr, rtc_ctrl->reg_size);
//     printd("RTC reg vir addr: %x\n", rtc_reg_va);

//     rtc_handler(RTC_CMD_INIT, rtc_reg_va, 0, 0);
//     uintptr_t init_time = rtc_handler(RTC_CMD_GET_TIME, 0, 0, 0);
//     printd("RTC time: %lld\n", init_time);

//     // for (uintptr_t time = rtc_handler(RTC_CMD_GET_TIME, 0, 0, 0) / 1000000000;
//     //      (time - init_time / 1000000000) < 3;
//     //      time = rtc_handler(RTC_CMD_GET_TIME, 0, 0, 0) / 1000000000)
//     //     printd("Count down: %d\r", 3 - (time - init_time / 1000000000));

//     return rtc_ctrl;
// }