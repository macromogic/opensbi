/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#include <sbi/sbi_console.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_scratch.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/reset/fdt_reset.h>
#include <sbi_utils/gpio/fdt_gpio.h>

struct poweroff_gpio pwroff = {
	.act_delay = 100,
	.gpio = -1,
	.output_type = GPIO_ACTIVE_LOW,
};

static int sifive_reset_init(void *fdt, int nodeoff,
			     const struct fdt_match *match)
{
	int rc;

	rc = fdt_parse_gpio_pwroff(fdt, &pwroff, "/gpio-poweroff");
	if (rc)
		sbi_printf("Warning: no gpio-poweroff definition in DT\n");

	return rc;
}

static void sifive_system_reset(u32 type, u32 reason)
{
	if (type == SBI_SRST_RESET_TYPE_SHUTDOWN) {
		if (pwroff.gpio < 0)
			return;

		gpio_direction_output(pwroff.gpio, 0);

		/* TODO:
		 * hold active if 'active-delay-ms' is in DT
		 */

		/* if success, power is off now. */
		while (1)
			wfi();
	} else {
		/* For now nothing to do. */
	}

	return;
}

static const struct fdt_match sifive_reset_match[] = {
	{ .compatible = "gpio-poweroff" },
	{ },
};

struct fdt_reset fdt_reset_sifive = {
	.match_table = sifive_reset_match,
	.init = sifive_reset_init,
	.system_reset = sifive_system_reset
};
