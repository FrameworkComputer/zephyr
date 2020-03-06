/*
 * Copyright (c) 2020 Framework Computer
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_INA230_H_
#define ZEPHYR_DRIVERS_SENSOR_INA230_H_

#include <kernel.h>

struct sensor_ina230_config {
	const char * i2c_master_dev_name;
	const u8_t i2c_slave_addr;
	const u8_t averages;
	const u8_t shunt_conversion_time;
	const u8_t vbus_conversion_time;
	const u8_t mode;
};

struct sensor_ina230_data {
			struct device *i2c_master;
			u16_t vbus_mv;
			s16_t power_mw;
			s16_t current_ma;
};

#endif /* ZEPHYR_DRIVERS_SENSOR_INA230_H_ */
