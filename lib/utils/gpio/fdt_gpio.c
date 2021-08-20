/*
 * SPDX-License-Identifier: GPL 2.0+
 *
 * Copyright (c) 2021 SiFive
 *
 * Authors:
 *   Green Wan <green.wan@sifive.com>
 */

#include <sbi/sbi_scratch.h>
#include <sbi/sbi_console.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/gpio/fdt_gpio.h>

extern struct fdt_gpio sifive_gpio;

static struct fdt_gpio *gpio_drivers[] = {
	&sifive_gpio
};

static struct fdt_gpio *current_driver = NULL;

int gpio_direction_output(unsigned gpio, int value)
{
	if (current_driver->direction_output)
		return current_driver->direction_output(gpio, value);

	return 0;
}

int fdt_gpio_init(void)
{
	int pos, noff, rc;
	struct fdt_gpio *drv;
	const struct fdt_match *match;
	void *fdt = sbi_scratch_thishart_arg1_ptr();

	for (pos = 0; pos < array_size(gpio_drivers); pos++) {
		drv = gpio_drivers[pos];

		noff = -1;
		while ((noff = fdt_find_match(fdt, noff,
					drv->match_table, &match)) >= 0) {
			if (drv->init) {
				rc = drv->init(fdt, noff, match);
				if (rc)
					return rc;
			}
			current_driver = drv;
		}
	}

	return 0;
}
