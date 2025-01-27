// See LICENSE for license details.

#ifndef _DRV_ENCLAVE_CONSOLE_H
#define _DRV_ENCLAVE_CONSOLE_H

#include <stdint.h>
#include <sbi/ebi/drv.h>
#include "../util/drv_ctrl.h"

#define CONSOLE_CMD_INIT 0
#define CONSOLE_CMD_PUT 1
#define CONSOLE_CMD_GET 2
#define CONSOLE_CMD_DESTORY 3
#define CONSOLE_REG_ADDR 0x10000000
#define CONSOLE_REG_SIZE 0x400
#define SUNXI_UART_BASE 0x02500000

#endif
