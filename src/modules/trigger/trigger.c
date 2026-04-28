/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/drivers/uart.h>
#include <stdbool.h>
#include <string.h>

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
static struct k_sem uart_sem; // created semaphore
uint8_t rx_byte;
char rx_buf[RX_BUF_SIZE];
static int index = 0;

static bool is_whitespace(char c)
{
	return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
}

static bool is_json_object(const char *input)
{
	if (input == NULL)
	{
		return false;
	}

	size_t len = strlen(input);
	size_t start = 0;
	size_t end = len;

	while ((start < len) && is_whitespace(input[start]))
	{
		start++;
	}

	while ((end > start) && is_whitespace(input[end - 1]))
	{
		end--;
	}

	if ((end - start) < 2)
	{
		return false;
	}

	if ((input[start] != '{') || (input[end - 1] != '}'))
	{
		return false;
	}

	bool in_string = false;
	bool escape = false;
	int object_depth = 0;
	int array_depth = 0;

	for (size_t i = start; i < end; i++)
	{
		char c = input[i];

		if (in_string)
		{
			if (escape)
			{
				escape = false;
			}
			else if (c == '\\')
			{
				escape = true;
			}
			else if (c == '"')
			{
				in_string = false;
			}

			continue;
		}

		switch (c)
		{
		case '"':
			in_string = true;
			break;
		case '{':
			object_depth++;
			break;
		case '}':
			if (object_depth == 0)
			{
				return false;
			}

			object_depth--;
			if ((object_depth == 0) && (i != (end - 1)))
			{
				return false;
			}
			break;
		case '[':
			array_depth++;
			break;
		case ']':
			if (array_depth == 0)
			{
				return false;
			}

			array_depth--;
			break;
		default:
			break;
		}
	}

	return (!in_string && !escape && (object_depth == 0) && (array_depth == 0));
}

static void uart_handler(const struct device *dev, void *data)
{
	ARG_UNUSED(data);

	if (!uart_irq_update(dev) || !uart_irq_rx_ready(dev))
	{
		return;
	}

	while (uart_fifo_read(dev, &rx_byte, 1) == 1)
	{
		if ((rx_byte == '\n') || (rx_byte == '\r'))
		{
			/* Accept both CR and LF line endings; ignore empty delimiters. */
			if (index > 0)
			{
				memset(payload.string, 0, sizeof(payload.string));
				snprintk(payload.string, sizeof(payload.string), "%s", rx_buf);
				memset(rx_buf, 0, sizeof(rx_buf));
				index = 0;
				k_sem_give(&uart_sem); // release semaphore when a full line arrives
			}

			continue;
		}

		if (index < RX_BUF_SIZE - 1)
		{
			rx_buf[index] = rx_byte;
			index++;
		}
		else
		{
			LOG_ERR("UART RX ERROR");
			SEND_FATAL_ERROR();
		}
	}
}

static int uart_init(void)
{
	k_sem_init(&uart_sem, 0, 1);
	if (!device_is_ready(dev))
	{
		LOG_ERR("%s device not ready", dev->name);
		return -ENODEV;
	}

	LOG_INF("Trigger UART ready on %s", dev->name);

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
		k_sem_take(&uart_sem, K_FOREVER); // take semaphore
		if ((payload.string[0] != '\0' && strlen(payload.string) > 0))
		{
			if (!is_json_object(payload.string))
			{
				LOG_WRN("Dropped non-JSON UART payload");
				memset(payload.string, 0, sizeof(payload.string));
				continue;
			}

			err = zbus_chan_pub(&MQTT_CHAN, &payload, K_SECONDS(10));
			if (err)
			{
				LOG_ERR("zbus_chan_pub, error:%d", err);
				SEND_FATAL_ERROR();
			}
			memset(payload.string, 0, sizeof(payload.string));
			LOG_DBG("Forwarded UART JSON payload to MQTT channel");
		}
		//k_sleep(K_MINUTES(1));
	}
}
K_THREAD_DEFINE(trigger_task_id,
				CONFIG_MQTT_SAMPLE_TRIGGER_THREAD_STACK_SIZE,
				trigger_task, NULL, NULL, NULL, 3, 0, 0);
