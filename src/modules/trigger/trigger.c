/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/drivers/uart.h>

#if CONFIG_DK_LIBRARY
#include <dk_buttons_and_leds.h>
#endif /* CONFIG_DK_LIBRARY */

#include "message_channel.h"
#define RX_BUF_SIZE 512

static const struct device *const dev = DEVICE_DT_GET(DT_NODELABEL(uart0));

#define UART_CFG                              \
	((struct uart_config){                    \
		.baudrate = 115200,                   \
		.parity = UART_CFG_PARITY_NONE,       \
		.stop_bits = UART_CFG_STOP_BITS_1,    \
		.data_bits = UART_CFG_DATA_BITS_8,    \
		.flow_ctrl = UART_CFG_FLOW_CTRL_NONE, \
	})

/* Register log module */
LOG_MODULE_REGISTER(trigger, CONFIG_MQTT_SAMPLE_TRIGGER_LOG_LEVEL);
struct velopera_payload payload = {0};
int len;
static struct k_sem uart_sem; // created semaphore
uint8_t rx_byte;
char rx_buf[RX_BUF_SIZE];
static int index = 0;
static void uart_handler(const struct device *dev, void *data)
{

	int err;
	uart_fifo_read(dev, &rx_byte, 1);

	if (rx_byte == '\n')
	{
		memset(payload.string, 0, sizeof(payload.string));
		len = snprintk(payload.string, sizeof(payload.string), "%s", rx_buf);
		LOG_INF("line %d  rxbufTail=%d rxbuf=%s", __LINE__, index, rx_buf);

		index = 0;
	}
	else if (index < RX_BUF_SIZE - 1)
	{
		rx_buf[index] = rx_byte;
		index++;
	}
	else
	{
		LOG_ERR("UART RX ERROR");
		SEND_FATAL_ERROR();
	}

	k_sem_give(&uart_sem); // release semaphore for sleep mode
}

static int uart_init(void)
{
	k_sem_init(&uart_sem, 0, 1);
	if (!device_is_ready(dev))
	{
		LOG_ERR("%s device not ready", dev->name);
		return -ENODEV;
	}

	uart_irq_callback_user_data_set(dev, uart_handler, NULL);

	struct uart_config uart_cfg = UART_CFG;
	/* Configure UART parameters */
	int err = uart_configure(dev, &uart_cfg);
	if (err)
	{
		LOG_ERR("uart_configure, error: %d", err);
		return err;
	}
	/* Enable RX interrupt */
	uart_irq_rx_enable(dev);

	return 0;
}

char *message_payload;
static void trigger_task(void)
{
	int err = uart_init();
	if (err)
	{
		LOG_ERR("uart_init, error: %d", err);
		SEND_FATAL_ERROR();
	}

	while (true)
	{
		if ((payload.string[0] != '\0' && strlen(payload.string) > 0))
		{
			err = zbus_chan_pub(&MQTT_CHAN, &payload, K_SECONDS(10));
			if (err)
			{
				LOG_ERR("zbus_chan_pub, error:%d", err);
				SEND_FATAL_ERROR();
			}
			memset(payload.string, 0, sizeof(payload.string));
		}
		k_sem_take(&uart_sem, K_FOREVER); // take semaphore
	}
}
K_THREAD_DEFINE(trigger_task_id,
				CONFIG_MQTT_SAMPLE_TRIGGER_THREAD_STACK_SIZE,
				trigger_task, NULL, NULL, NULL, 3, 0, 0);
