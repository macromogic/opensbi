/*
 * SPDX-License-Identifier: GPL 2.0+
 *
 * Copyright (c) 2021 SiFive Inc.
 *
 * Authors:
 *   Green Wan <green.wan@sifive.com>
 */

#ifndef __FDT_GPIO_H__
#define __FDT_GPIO_H__

#include <sbi/sbi_types.h>

/**
 * Make a GPIO an output, and set its value.
 *
 * @param gpio  GPIO number
 * @param value GPIO value (0 for low or 1 for high)
 * @return 0 if ok, -1 on error
 */
int gpio_direction_output(unsigned gpio, int value);

struct fdt_gpio {
	const struct fdt_match *match_table;
	int (*init)(void *fdt, int nodeoff, const struct fdt_match *match);
	int (*direction_output)(unsigned int gpio, int value);
};

int fdt_gpio_init(void);

#endif
