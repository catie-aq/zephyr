/*
 * Copyright (c) 2017, NXP
 * Copyright (c) 2025, CATIE
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor_data_types.h>
#include <zephyr/dsp/print_format.h>
#include <zephyr/logging/log.h>
#include <zephyr/rtio/rtio.h>
#include <stdio.h>

LOG_MODULE_REGISTER(async_heart_rate, LOG_LEVEL_INF);

SENSOR_DT_READ_IODEV(iodev, DT_ALIAS(heart_rate_sensor),
#if CONFIG_MAX30101_DIE_TEMPERATURE
		{SENSOR_CHAN_DIE_TEMP, 0},
#endif /* CONFIG_MAX30101_DIE_TEMPERATURE */
		{SENSOR_CHAN_RED, 0},
		{SENSOR_CHAN_IR, 0},
		{SENSOR_CHAN_GREEN, 0});
RTIO_DEFINE(ctx, 1, 1);

#include <zephyr/drivers/gpio.h>
const struct gpio_dt_spec sensor_ctrl = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), ctrl_gpios);

static const struct device *check_heart_rate_device(void)
{
	const struct device *const dev = DEVICE_DT_GET(DT_ALIAS(heart_rate_sensor));

	if (dev == NULL) {
		/* No such node, or the node does not have status "okay". */
		LOG_ERR("Error: no device found.");
		return NULL;
	}

	if (!device_is_ready(dev)) {
		LOG_ERR("Error: Device \"%s\" is not ready; "
		       "check the driver initialization logs for errors.",
		       dev->name);
		return NULL;
	}

	/* Setup CTLR */
	if (!gpio_is_ready_dt(&sensor_ctrl)) {
        LOG_ERR("ERROR: PPG CTRL device is not ready");
		return NULL;
	}

	int result = gpio_pin_configure_dt(&sensor_ctrl, GPIO_OUTPUT_ACTIVE);
	if (result < 0) {
        LOG_ERR("ERROR [%d]: Failed to configure PPG CTRL gpio", result);
		return NULL;
	}

	LOG_INF("Found device \"%s\", getting sensor data", dev->name);
	return dev;
}

int main(void)
{
	const struct device *dev = check_heart_rate_device();

	if (dev == NULL) {
		return 0;
	}

	while (1) {
		uint8_t buf[128];

		int rc = sensor_read(&iodev, &ctx, buf, 128);

		if (rc != 0) {
			LOG_ERR("%s: sensor_read() failed: %d", dev->name, rc);
			return rc;
		}

		const struct sensor_decoder_api *decoder;

		rc = sensor_get_decoder(dev, &decoder);

		if (rc != 0) {
			LOG_ERR("%s: sensor_get_decode() failed: %d", dev->name, rc);
			return rc;
		}

		uint32_t red_fit = 0, ir_fit = 0, green_fit = 0;
		struct sensor_q31_data red_data = {0}, ir_data = {0}, green_data = {0};

		decoder->decode(buf,
			(struct sensor_chan_spec) {SENSOR_CHAN_RED, 0},
			&red_fit, 1, &red_data);

		decoder->decode(buf,
				(struct sensor_chan_spec) {SENSOR_CHAN_IR, 0},
				&ir_fit, 1, &ir_data);

		decoder->decode(buf,
				(struct sensor_chan_spec) {SENSOR_CHAN_GREEN, 0},
				&green_fit, 1, &green_data);

		LOG_INF("RED: %" PRIsensor_q31_data "; IR: %" PRIsensor_q31_data "; GREEN: %" PRIsensor_q31_data,
			PRIsensor_q31_data_arg(red_data, 0),
			PRIsensor_q31_data_arg(ir_data, 0),
			PRIsensor_q31_data_arg(green_data, 0));

#if CONFIG_MAX30101_DIE_TEMPERATURE
		uint32_t temp_fit = 0;
		struct sensor_q31_data temp_data = {0};
		decoder->decode(buf,
				(struct sensor_chan_spec) {SENSOR_CHAN_DIE_TEMP, 0},
				&temp_fit, 1, &temp_data);

		LOG_INF("TEMP: %" PRIsensor_q31_data, PRIsensor_q31_data_arg(temp_data, 0));
#endif /* CONFIG_MAX30101_DIE_TEMPERATURE */

		k_sleep(K_MSEC(100));
	}
	return 0;
}
