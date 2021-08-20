/*
 * SPDX-License-Identifier: GPL 2.0+
 *
 * Copyright (c) 2021 SiFive.
 *
 * Authors:
 *   Green Wan <green.wan@sifive.com>
 */

#ifndef __SBI_GPIO_H__
#define __SBI_GPIO_H__

#include <sbi/sbi_types.h>

struct sbi_scratch;

/* Initialize gpio */
int sbi_gpio_init(struct sbi_scratch *scratch, bool cold_boot);

/* Exit gpio */
void sbi_gpio_exit(struct sbi_scratch *scratch);

#endif
