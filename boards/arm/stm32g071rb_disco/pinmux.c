/*
 * Copyright (c) 2020 Framework Computer LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <init.h>
#include <soc.h>
#include <pinmux/stm32/pinmux_stm32.h>

/* pin assignments for STM32G071RB-DISCO board */
static const struct pin_config pinconf[] = {

};

static int pinmux_stm32_init(struct device *port)
{
	ARG_UNUSED(port);

	stm32_setup_pins(pinconf, ARRAY_SIZE(pinconf));

	return 0;
}

SYS_INIT(pinmux_stm32_init, PRE_KERNEL_1,
	 CONFIG_PINMUX_STM32_DEVICE_INITIALIZATION_PRIORITY);
