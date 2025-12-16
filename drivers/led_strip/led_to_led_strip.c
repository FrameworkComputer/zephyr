#include <zephyr/kernel.h>
#include <zephyr/drivers/led_strip.h> // The API we implement
#include <zephyr/drivers/led.h>       // The API we consume
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(led_to_strip_adapter);

#define DT_DRV_COMPAT led_to_led_strip

struct lts_config {
    const struct device *led_dev; // The IS31FL3743A device
    uint32_t num_pixels;
    const uint32_t *map;          // Pointer to the flat color-map array
};

/* * Helper: Convert 0-255 (Strip API) to 0-100 (LED API)
 */
static inline uint8_t scale_255_to_100(uint8_t val)
{
    // Simple integer math: (val * 100) / 255
    return (val * 100) / 255;
}

/*
 * The Core Function: Update RGB
 */
static int lts_update_rgb(const struct device *dev, struct led_rgb *pixels, size_t num_pixels)
{
    const struct lts_config *config = dev->config;
    int ret = 0;

    // Sanity check
    if (num_pixels > config->num_pixels) {
        return -ENOMEM;
    }

    // Iterate through every "Pixel" the app wants to set
    //for (size_t i = 0; i < num_pixels; i++) {
    for (size_t i = 0; i < num_pixels; i+=3) {

        // Calculate the index in the flat mapping array
        // Map structure is [R0, G0, B0, R1, G1, B1, ...]
        size_t map_idx = i * 3;

        // uint32_t r_channel = config->map[map_idx];
        // uint32_t g_channel = config->map[map_idx + 1];
        // uint32_t b_channel = config->map[map_idx + 2];
        uint32_t r_channel = i;
        uint32_t g_channel = i+1;
        uint32_t b_channel = i+2;

        // 1. Set RED
        // Note: Generic LED API usually uses 0-100% brightness
        ret = led_set_brightness(config->led_dev, r_channel, scale_255_to_100(pixels[i].r));
        if (ret < 0) return ret;

        // 2. Set GREEN
        ret = led_set_brightness(config->led_dev, g_channel, scale_255_to_100(pixels[i].g));
        if (ret < 0) return ret;

        // 3. Set BLUE
        ret = led_set_brightness(config->led_dev, b_channel, scale_255_to_100(pixels[i].b));
        if (ret < 0) return ret;
    }

    // Some LED controllers need an explicit commit/flush.
    // The generic LED API doesn't have a flush, but if IS31FL3743A
    // buffers changes, you might need a custom call here or ensure
    // the driver writes immediately.

    return 0;
}

static int lts_update_channels(const struct device *dev, uint8_t *channels, size_t num_channels)
{
    // This API is rarely used for RGB strips, usually for raw white strips.
    // Implementing it would require a separate map for 1:1 channel mapping.
    return -ENOTSUP;
}

/* API Structure */
static const struct led_strip_driver_api lts_api = {
    .update_rgb = lts_update_rgb,
    .update_channels = lts_update_channels,
};

/* Initialization */
static int lts_init(const struct device *dev)
{
    const struct lts_config *config = dev->config;

    if (!device_is_ready(config->led_dev)) {
        LOG_ERR("Underlying LED device not ready");
        return -ENODEV;
    }
    return 0;
}

/* Macro to instantiate the driver */
//    static const uint32_t map_##inst[] = DT_INST_PROP(inst, color_map);
//        .map = map_##inst,
#define LTS_DEFINE(inst)                                            \
                                                                    \
    static const struct lts_config lts_config_##inst = {            \
        .led_dev = DEVICE_DT_GET(DT_INST_PHANDLE(inst, led_controller)), \
        .num_pixels = DT_INST_PROP(inst, chain_length),             \
    };                                                              \
                                                                    \
    DEVICE_DT_INST_DEFINE(inst,                                     \
        lts_init,                                                   \
        NULL,                                                       \
        NULL,                                                       \
        &lts_config_##inst,                                         \
        POST_KERNEL, CONFIG_LED_STRIP_INIT_PRIORITY,                \
        &lts_api);

DT_INST_FOREACH_STATUS_OKAY(LTS_DEFINE)
