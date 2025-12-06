/*
 * Copyright 2022-2023 Daniel DeGrasse <daniel@degrasse.com>
 * Copyright 2025-2026 Framework Computer Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define DT_DRV_COMPAT issi_is31fl3741

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/drivers/led/is31fl3741.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(is31fl3741, CONFIG_LED_LOG_LEVEL);

/* IS31FL3741 register definitions */
#define CMD_SEL_REG   0xFD /* Command/page selection reg */
#define CMD_SEL_PWM0  0x00 /* PWM configuration page 0 */
#define CMD_SEL_PWM1  0x01 /* PWM configuration page 1 */
#define CMD_SEL_SCAL0 0x02 /* Scaling configuration page 0 */
#define CMD_SEL_SCAL1 0x03 /* Scaling configuration page 1 */
#define CMD_SEL_FUNC  0x04 /* Function configuration page */

#define CMD_LOCK_REG    0xFE /* Command selection lock reg */
#define CMD_LOCK_UNLOCK 0xC5 /* Command sel unlock value */

/* IS31FL3741 page specific register definitions */

/* Function configuration page */
#define CONF_REG           0x00 /* configuration register */
#define CONF_REG_SSD_MASK  0x01 /* Software shutdown mask */
#define CONF_REG_SSD_SHIFT 0x00 /* Software shutdown shift */
#define CONF_REG_SWS_SHIFT 0x04 /* Sync mode shift */
#define CONF_REG_SWS_MASK  0x0F /* Sync mode mask */

#define GLOBAL_CURRENT_CTRL_REG 0x01 /* global current control register */

#define PWM_FREQ_REGISTER 0x36 /* PWM frequency setting register */

#define RESET_REG 0x3F /* Reset all registers to POR state */
#define RESET_CMD 0xAE /* Value to write to RESET_REG */

/* Matrix Layout definitions */
#define IS31FL3741_SW_COUNT 9  /* Rows */
#define IS31FL3741_CS_COUNT 39 /* Columns */
#define IS31FL3741_MAX_LED  (IS31FL3741_SW_COUNT * IS31FL3741_CS_COUNT)

/* Buffer sizes: Register Byte + Data Bytes */
/* Page 0 covers SW1-SW6 for CS1-CS30 (6 * 30 = 180 LEDs) */
#define IS31FL3741_PAGE0_SIZE 180
/* Page 1 covers SW7-SW9 for CS1-CS30 (3 * 30 = 90) + SW1-SW9 for CS31-CS39 (9 * 9 = 81) = 171 LEDs
 */
#define IS31FL3741_PAGE1_SIZE 171

struct is31fl3741_config {
	struct i2c_dt_spec bus;
	struct gpio_dt_spec sdb;
	uint8_t current_limit;
	int pwm_frequency;
	uint8_t current_sources;
};

struct is31fl3741_data {
	/* Active configuration page */
	uint32_t selected_page;

	/* Shadow buffers, used for bulk controller writes.
	 * Index 0 is the register address (start address of the page).
	 */
	uint8_t page0_buf[IS31FL3741_PAGE0_SIZE + 1];
	uint8_t page1_buf[IS31FL3741_PAGE1_SIZE + 1];
};

/* Selects target register page for IS31FL3741 */
static int is31fl3741_select_page(const struct device *dev, uint8_t page)
{
	const struct is31fl3741_config *config = dev->config;
	struct is31fl3741_data *data = dev->data;
	int ret = 0;

	if (data->selected_page == page) {
		return 0;
	}

	/* Unlock page selection register */
	ret = i2c_reg_write_byte_dt(&config->bus, CMD_LOCK_REG, CMD_LOCK_UNLOCK);
	if (ret < 0) {
		LOG_ERR("Could not unlock page selection register");
		return ret;
	}

	/* Write to function select to select active page */
	ret = i2c_reg_write_byte_dt(&config->bus, CMD_SEL_REG, page);
	if (ret < 0) {
		LOG_ERR("Could not select active page");
		return ret;
	}
	data->selected_page = page;

	return ret;
}

/**
 * @brief Map a linear channel ID to the hardware Page and Register.
 *
 * Mapping logic based on Datasheet Figure 8 and 9.
 * The matrix is 9 Rows (SW) x 39 Columns (CS).
 * Zephyr linear mapping assumed: Channel = (SW * 39) + CS.
 */
static void is31fl3741_map(uint32_t led, uint8_t *page, uint8_t *reg)
{
	uint8_t sw = led / IS31FL3741_CS_COUNT; // 0 to 8
	uint8_t cs = led % IS31FL3741_CS_COUNT; // 0 to 38

	if (cs < 30) {
		/* CS1 to CS30 (Indices 0-29) */
		if (sw < 6) {
			/* SW1-SW6: Page 0 */
			*page = 0;
			*reg = sw * 30 + cs;
		} else {
			/* SW7-SW9: Page 1 */
			*page = 1;
			*reg = (sw - 6) * 30 + cs;
		}
	} else {
		/* CS31 to CS39 (Indices 30-38) are all in Page 1 */
		*page = 1;
		/* Offset starts at 0x5A (90) */
		*reg = 0x5A + (sw * 9) + (cs - 30);
	}
}

static int flush_buffers(const struct device *dev)
{
	struct is31fl3741_data *data = dev->data;
	const struct is31fl3741_config *config = dev->config;
	int ret;

	/* Write Page 0 (PWM 1) */
	ret = is31fl3741_select_page(dev, CMD_SEL_PWM0);
	if (ret < 0) {
		return ret;
	}
	ret = i2c_write_dt(&config->bus, data->page0_buf, IS31FL3741_PAGE0_SIZE + 1);
	if (ret < 0) {
		return ret;
	}

	/* Write Page 1 (PWM 2) */
	ret = is31fl3741_select_page(dev, CMD_SEL_PWM1);
	if (ret < 0) {
		return ret;
	}
	ret = i2c_write_dt(&config->bus, data->page1_buf, IS31FL3741_PAGE1_SIZE + 1);

	return ret;
}

static int is31fl3741_led_set_brightness(const struct device *dev, uint32_t led, uint8_t value)
{
	const struct is31fl3741_config *config = dev->config;
	struct is31fl3741_data *data = dev->data;
	uint8_t page, reg;
	int ret;

	if (led >= IS31FL3741_MAX_LED) {
		return -EINVAL;
	}

	is31fl3741_map(led, &page, &reg);

	if (page == 0) {
		/* +1 to account for register address at index 0 */
		data->page0_buf[1 + reg] = value;
		ret = is31fl3741_select_page(dev, CMD_SEL_PWM0);
	} else {
		data->page1_buf[1 + reg] = value;
		ret = is31fl3741_select_page(dev, CMD_SEL_PWM1);
	}

	if (ret < 0) {
		return ret;
	}

	return i2c_reg_write_byte_dt(&config->bus, reg, value);
}

static int is31fl3741_led_write_channels(const struct device *dev, uint32_t start_channel,
					 uint32_t num_channels, const uint8_t *buf)
{
	struct is31fl3741_data *data = dev->data;
	uint8_t page, reg;

	if ((start_channel + num_channels) > IS31FL3741_MAX_LED) {
		return -EINVAL;
	}

	/* Update shadow buffers */
	for (int i = 0; i < num_channels; i++) {
		is31fl3741_map(start_channel + i, &page, &reg);
		if (page == 0) {
			data->page0_buf[1 + reg] = buf[i];
		} else {
			data->page1_buf[1 + reg] = buf[i];
		}
	}

	/* Flush entire matrix to hardware */
	return flush_buffers(dev);
}

static int is31fl3741_init(const struct device *dev)
{
	const struct is31fl3741_config *config = dev->config;
	struct is31fl3741_data *data = dev->data;
	int ret = 0;

	if (!i2c_is_ready_dt(&config->bus)) {
		LOG_ERR("I2C device not ready");
		return -ENODEV;
	}

	if (config->sdb.port != NULL) {
		if (!gpio_is_ready_dt(&config->sdb)) {
			LOG_ERR("GPIO SDB pin not ready");
			return -ENODEV;
		}
		/* Set SDB pin high to exit hardware shutdown */
		ret = gpio_pin_configure_dt(&config->sdb, GPIO_OUTPUT_ACTIVE);
		if (ret < 0) {
			return ret;
		}
		k_sleep(K_MSEC(10)); /* Wait for chip to wake up */
	}

	/* Select function page */
	ret = is31fl3741_select_page(dev, CMD_SEL_FUNC);
	if (ret < 0) {
		return ret;
	}

	/* Reset registers to POR state */
	ret = i2c_reg_write_byte_dt(&config->bus, RESET_REG, RESET_CMD);
	if (ret < 0) {
		LOG_ERR("Failed to reset registers");
		return ret;
	}
	k_sleep(K_MSEC(10)); /* Wait for reset to complete */

	/* Re-select function page after reset */
	ret = is31fl3741_select_page(dev, CMD_SEL_FUNC);
	if (ret < 0) {
		return ret;
	}

	/* Set global current control register */
	ret = i2c_reg_write_byte_dt(&config->bus, GLOBAL_CURRENT_CTRL_REG, config->current_limit);
	if (ret < 0) {
		LOG_ERR("Failed to write GCC");
		return ret;
	}

	/* Set PWM frequency */
	ret = is31fl3741_pwm_frequency(dev, config->pwm_frequency);
	if (ret < 0) {
		LOG_ERR("Failed to set PWM");
		return ret;
	}

	/* Exit software shutdown and set SWS (Sync Mode) */
	ret = i2c_reg_write_byte_dt(&config->bus, CONF_REG,
				    (config->current_sources << CONF_REG_SWS_SHIFT) |
					    CONF_REG_SSD_MASK);
	if (ret < 0) {
		LOG_ERR("Failed to set SSD");
		return ret;
	}

	/* Set up Scaling Registers (Page 2 & 3) to Max (0xFF) */
	data->page0_buf[0] = 0x00; /* Register address 0 */
	memset(data->page0_buf + 1, 0xFF, IS31FL3741_PAGE0_SIZE);

	data->page1_buf[0] = 0x00; /* Register address 0 */
	memset(data->page1_buf + 1, 0xFF, IS31FL3741_PAGE1_SIZE);

	/* Write Scaling Page 0 (mapped to CMD_SEL_SCAL0) */
	ret = is31fl3741_select_page(dev, CMD_SEL_SCAL0);
	if (ret < 0) {
		return ret;
	}
	ret = i2c_write_dt(&config->bus, data->page0_buf, IS31FL3741_PAGE0_SIZE + 1);
	if (ret < 0) {
		return ret;
	}

	/* Write Scaling Page 1 (mapped to CMD_SEL_SCAL1) */
	ret = is31fl3741_select_page(dev, CMD_SEL_SCAL1);
	if (ret < 0) {
		return ret;
	}
	ret = i2c_write_dt(&config->bus, data->page1_buf, IS31FL3741_PAGE1_SIZE + 1);
	if (ret < 0) {
		return ret;
	}

	/* Clear buffers to 0x00 for PWM control (LEDs Off initially) */
	memset(data->page0_buf + 1, 0x00, IS31FL3741_PAGE0_SIZE);
	memset(data->page1_buf + 1, 0x00, IS31FL3741_PAGE1_SIZE);

	/* Flush zeros to PWM registers */
	ret = flush_buffers(dev);

	return ret;
}

/* Custom IS31FL3741 specific APIs */

int is31fl3741_blank(const struct device *dev, bool blank_en)
{
	const struct is31fl3741_config *config = dev->config;
	int ret;

	ret = is31fl3741_select_page(dev, CMD_SEL_FUNC);
	if (ret < 0) {
		return ret;
	}

	uint8_t val = blank_en ? 0 : CONF_REG_SSD_MASK;
	return i2c_reg_update_byte_dt(&config->bus, CONF_REG, CONF_REG_SSD_MASK, val);
}

int is31fl3741_pwm_frequency(const struct device *dev, int limit)
{
	const struct is31fl3741_config *config = dev->config;
	int ret;

	ret = is31fl3741_select_page(dev, CMD_SEL_FUNC);
	if (ret < 0) {
		return ret;
	}

	uint8_t reg_setting = 0x00;
	switch (limit) {
	case 29000:
		reg_setting = 0x00;
		break;
	case 3600:
		reg_setting = 0x03;
		break;
	case 1800:
		reg_setting = 0x07;
		break;
	default:
		return -EINVAL;
	}

	/* Set frequency setting register */
	return i2c_reg_write_byte_dt(&config->bus, PWM_FREQ_REGISTER, reg_setting);
}

int is31fl3741_current_limit(const struct device *dev, uint8_t limit)
{
	const struct is31fl3741_config *config = dev->config;
	int ret;

	ret = is31fl3741_select_page(dev, CMD_SEL_FUNC);
	if (ret < 0) {
		return ret;
	}

	/* Set global current control register */
	return i2c_reg_write_byte_dt(&config->bus, GLOBAL_CURRENT_CTRL_REG, limit);
}

static DEVICE_API(led, is31fl3741_api) = {
	.set_brightness = is31fl3741_led_set_brightness,
	.write_channels = is31fl3741_led_write_channels,
};

#define IS31FL3741_DEVICE(n)                                                                       \
	static const struct is31fl3741_config is31fl3741_config_##n = {                            \
		.bus = I2C_DT_SPEC_INST_GET(n),                                                    \
		.sdb = GPIO_DT_SPEC_INST_GET_OR(n, sdb_gpios, {}),                                 \
		.current_limit = DT_INST_PROP(n, current_limit),                                   \
		.pwm_frequency = DT_INST_PROP(n, pwm_frequency),                                   \
		.current_sources = DT_INST_ENUM_IDX(n, current_sources),                           \
	};                                                                                         \
                                                                                                   \
	static struct is31fl3741_data is31fl3741_data_##n = {                                      \
		.selected_page = CMD_SEL_PWM0,                                                     \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, &is31fl3741_init, NULL, &is31fl3741_data_##n,                     \
			      &is31fl3741_config_##n, POST_KERNEL, CONFIG_LED_INIT_PRIORITY,       \
			      &is31fl3741_api);

DT_INST_FOREACH_STATUS_OKAY(IS31FL3741_DEVICE)
