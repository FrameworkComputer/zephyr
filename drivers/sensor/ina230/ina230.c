/*
 * Copyright (c) 2020 Framework Computer LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <device.h>
#include <drivers/i2c.h>
#include <drivers/gpio.h>
#include <kernel.h>
#include <drivers/sensor.h>
#include <sys/util.h>
#include <sys/__assert.h>
#include <logging/log.h>

#include "ina230.h"


#define INA230_REG_CONFIG        0x00
#define INA230_REG_VSHUNT        0x01
#define INA230_REG_VBUS          0x02
#define INA230_REG_PWR           0x03
#define INA230_REG_CURRENT       0x04
#define INA230_REG_CALIBRATION   0x05
#define INA230_REG_MASK_ENABLE   0x06
#define INA230_REG_ALERT_LIMIT   0x07
#define INA230_REG_ID            0xFF

#define INA230_MODE_POWER_DOWN       0x00
#define INA230_MODE_TRIGGERED_VSHUNT 0x01
#define INA230_MODE_TRIGGERED_VBUS   0x02
#define INA230_MODE_TRIGGERED_ALL    0x03
#define INA230_MODE_CONTINOUS_VSHUNT 0x05
#define INA230_MODE_CONTINOUS_VBUS   0x06
#define INA230_MODE_CONTINOUS_ALL    0x07


#define INA230_REG_CONFIG_RESET 0x8000
union ina230_reg_config_u {
	u16_t w;
	struct ina230_reg_config_t {
		u16_t reset : 1;
		u16_t : 3;
		u16_t average : 3;
		u16_t vbus_conversion_time : 3;
		u16_t shunt_conversion_time : 3;
		u16_t mode : 3;
	} r;
};


LOG_MODULE_REGISTER(INA230, CONFIG_SENSOR_LOG_LEVEL);


static int sensor_ina230_read16(struct device *dev, u8_t a, u8_t d, u16_t * v)
{
	u8_t buf[2] = {};
	if (i2c_burst_read(dev, a, d, (u8_t *)buf, 2) < 0) {
		LOG_ERR("Error reading register.");
		return -EIO;
	}
	*v =  (buf[0] << 8 | buf[1]);
	return 0;
}
static int sensor_ina230_write16(struct device *dev, u8_t a, u16_t d, u16_t v)
{
	u8_t buf[2] = {0xFF & (v >> 8), v & 0xff};
	if (i2c_burst_write(dev, a, d, (u8_t *)buf, 2) < 0) {
		LOG_ERR("Error writing register.");
		return -EIO;
	}
	return 0;
}

static int sensor_ina230_init_chip(struct device *dev)
{
	const struct sensor_ina230_config * const config = dev->config->config_info;
	struct sensor_ina230_data *data = dev->driver_data;
	/* RESET */
	union ina230_reg_config_u cfg;
	sensor_ina230_read16(data->i2c_master, config->i2c_slave_addr,
			INA230_REG_CONFIG, &(cfg.w));
	cfg.r.reset = 1;
	sensor_ina230_write16(data->i2c_master, config->i2c_slave_addr,
			INA230_REG_CONFIG, cfg.w);
	cfg.r.reset = 0;
	cfg.r.average = config->averages;
	cfg.r.shunt_conversion_time = config->shunt_conversion_time;
	cfg.r.vbus_conversion_time = config->vbus_conversion_time;
	cfg.r.mode = config->mode;

	sensor_ina230_write16(data->i2c_master, config->i2c_slave_addr,
			INA230_REG_CONFIG, cfg.w);

	return 0;
}
static int sensor_ina230_init(struct device *dev)
{
	const struct sensor_ina230_config * const config = dev->config->config_info;
	struct sensor_ina230_data *data = dev->driver_data;

	data->i2c_master = device_get_binding(config->i2c_master_dev_name);
	if (!data->i2c_master) {
		LOG_DBG("i2c master not found: %s",
			    config->i2c_master_dev_name);
		return -EINVAL;
	}

	if (sensor_ina230_init_chip(dev) < 0) {
		LOG_DBG("failed to initialize chip");
		return -EIO;
	}

	return 0;
}

static int sensor_ina230_sample_fetch(struct device *dev,
				     enum sensor_channel chan)
{
	const struct sensor_ina230_config * const config = dev->config->config_info;
	struct sensor_ina230_data *data = dev->driver_data;

	__ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL ||
			chan == SENSOR_CHAN_GYRO_XYZ);
	uint16_t raw = 0;
	sensor_ina230_read16(data->i2c_master, config->i2c_slave_addr,
			INA230_REG_CURRENT, &raw);
	data->current_ma = raw;
	sensor_ina230_read16(data->i2c_master, config->i2c_slave_addr,
			INA230_REG_PWR, &raw);
	data->power_mw = raw*25;
	sensor_ina230_read16(data->i2c_master, config->i2c_slave_addr,
			INA230_REG_VBUS, &raw);
	data->vbus_mv = ((int32_t)raw * 1250 + 1000/2)/1000;

	return 0;
}

static int sensor_ina230_channel_get(struct device *dev,
				    enum sensor_channel chan,
				    struct sensor_value *val)
{
	struct sensor_ina230_data *data = dev->driver_data;
	switch(chan){
	case SENSOR_CHAN_VOLTAGE:
		val->val1 = data->vbus_mv / 1000;
		val->val2 = (data->vbus_mv % 1000) * 1000;
		break;

	case SENSOR_CHAN_CURRENT:
		val->val1 = data->current_ma / 1000;
		val->val2 = (data->current_ma % 1000) * 1000;
		break;

	case SENSOR_CHAN_POWER:
		val->val1 = data->power_mw / 1000;
		val->val2 = (data->power_mw % 1000) * 1000;
		break;
	default:
		return -ENOTSUP;
	}
	return 0;
}

static const struct sensor_driver_api sensor_ina230_api_funcs = {
	.sample_fetch = sensor_ina230_sample_fetch,
	.channel_get = sensor_ina230_channel_get,
};


#define INA230_INIT(index) \
static const struct sensor_ina230_config 	sensor_ina230_cfg_##index = {\
												.i2c_master_dev_name = 	DT_INST_BUS_LABEL(index),\
												.i2c_slave_addr =  DT_INST_PROP(index,base_address),\
												.averages =  DT_INST_PROP(index,averages),\
												.shunt_conversion_time =  DT_INST_PROP(index,shunt_conversion_time),\
												.vbus_conversion_time =  DT_INST_PROP(index,vbus_conversion_time),\
												.mode =  DT_INST_PROP(index,mode),\
											};	\
static struct sensor_ina230_data 		sensor_ina230_data_##index = {\
										};	\
	\
DEVICE_AND_API_INIT(sensor_ina230_##index, DT_INST_LABEL(index), \
		&sensor_ina230_init, \
		&sensor_ina230_data_##index, \
		&sensor_ina230_cfg_##index, \
		POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY, \
		&sensor_ina230_api_funcs); \


DT_INST_FOREACH(INA230_INIT)


