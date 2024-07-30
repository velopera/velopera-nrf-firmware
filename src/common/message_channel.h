/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _MESSAGE_CHANNEL_H_
#define _MESSAGE_CHANNEL_H_

#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log_ctrl.h>
#include <nrf_modem_gnss.h>

#ifdef __cplusplus
extern "C"
{
#endif
#define GNSS_DATA_JSON \
	"{\"Latitude\":\"%.06f\",\"Longitude\":\"%.06f\",\"Altitude\":\"%.01f\",\"Accuracy\":\"%.01f\",\"Speed\":\"%.01f\",\"Speed accuracy\":\"%.01f\",\"Heading\":\"%.01f\",\"Date\":\"%04u-%02u-%02u\",\"Time\":\"%02u:%02u:%02u.%03u\",\"PDOP\":\"%.01f\",\"HDOP\":\"%.01f\",\"VDOP\":\"%.01f\",\"TDOP\":\"%.01f\",\"measId\":%d}"

/** @brief Macro used to send a message on the FATAL_ERROR_CHANNEL.
 *	   The message will be handled in the error module.
 */
#define SEND_FATAL_ERROR()                                                         \
	int not_used = -1;                                                             \
	if (zbus_chan_pub(&FATAL_ERROR_CHAN, &not_used, K_SECONDS(10)))                \
	{                                                                              \
		LOG_ERR("Sending a message on the fatal error channel failed, rebooting"); \
		LOG_PANIC();                                                               \
		IF_ENABLED(CONFIG_REBOOT, (sys_reboot(0)));                                \
	}

	struct velopera_payload
	{
		char string[700];
	};
	struct velopera_gps_data
	{
		int meas_id;
		struct nrf_modem_gnss_pvt_data_frame pvt;
	};
	enum network_status
	{
		NETWORK_DISCONNECTED,
		NETWORK_CONNECTED,
	};

	struct fota_filename
	{
		/** Pointer to buffer. */
		char *ptr;

		/** Size of buffer. */
		size_t size;
	};

	/* Declare the zbus channels */
	ZBUS_CHAN_DECLARE(FOTA_CHAN, MQTT_CHAN, GPS_CHAN, NETWORK_CHAN, FATAL_ERROR_CHAN);

#endif /* _MESSAGE_CHANNEL_H_ */
