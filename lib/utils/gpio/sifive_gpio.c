/*
 * SPDX-License-Identifier: GPL 2.0+
 *
 * Copyright (c) 2021 SiFive Inc.
 *
 * Authors:
 *   Green Wan <green.wan@sifive.com>
 */

#include <sbi/riscv_io.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_console.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/gpio/fdt_gpio.h>
#include <sbi_utils/sys/clint.h>

#define GPIO_OUTEN				0x8
#define GPIO_OUTVAL				0xC
#define GPIO_BIT(b)				(1UL << (b))

static struct platform_gpio_data gpio_data = {
	.addr = 0x0
};

static int sifive_gpio_init(void *fdt, int nodeoff,
				  const struct fdt_match *match)
{
	int rc;

	rc = fdt_parse_gpio_node(fdt, nodeoff, &gpio_data);
	if (rc)
		return rc;

	return 0;
}

static int sifive_gpio_direction_output(unsigned int gpio, int value)
{
	if (gpio_data.addr != 0) {
		unsigned int val;

		val = readl((volatile void *)(gpio_data.addr + GPIO_OUTEN));
		val |= GPIO_BIT(gpio);
		writel(val, (volatile void *)(gpio_data.addr + GPIO_OUTEN));

		val = readl((volatile void *)(gpio_data.addr + GPIO_OUTEN));
		val &= ~GPIO_BIT(gpio);
		writel(val, (volatile void *)(gpio_data.addr + GPIO_OUTVAL));

		return 0;
	}

	return SBI_EINVAL;
}
static const struct fdt_match sifive_gpio_match[] = {
	{ .compatible = "sifive,gpio0" },
	{ },
};

struct fdt_gpio sifive_gpio = {
	.match_table = sifive_gpio_match,
	.init = sifive_gpio_init,
	.direction_output = sifive_gpio_direction_output,
};
