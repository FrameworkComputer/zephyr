#include <zephyr.h>
#include <device.h>
#include <sys/printk.h>

#include <drivers/gpio.h>

#include <shell/shell.h>
#include <shell/shell_uart.h>
#include <version.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

void main(void)
{
        struct device *dev;
        bool led_is_on = true;
        int ret;

        dev = device_get_binding(DT_ALIAS_LED0_GPIOS_CONTROLLER);
        if (dev == NULL) {
                return;
        }

        ret = gpio_pin_configure(dev, DT_ALIAS_LED0_GPIOS_PIN,
                                 GPIO_OUTPUT_ACTIVE
                                 | DT_ALIAS_LED0_GPIOS_FLAGS);
        if (ret < 0) {
                return;
        }
        /* initial placeholder, blink LED */ 
        while (1) {
                gpio_pin_set(dev, DT_ALIAS_LED0_GPIOS_PIN, (int)led_is_on);
                led_is_on = !led_is_on;
                k_sleep(SLEEP_TIME_MS);
        }
}