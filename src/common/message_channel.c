/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include "message_channel.h"

/* Define FOTA_CHAN */
ZBUS_CHAN_DEFINE(FOTA_CHAN,							  /* Name */
				 struct fota_filename,				  /* Message type */
				 NULL,								  /* Validator */
				 NULL,								  /* User data */
				 ZBUS_OBSERVERS(fota_app, transport), /* Observers */
				 ZBUS_MSG_INIT(0)					  /* Initial value {0} */
);

/* Define MQTT_CHAN */
ZBUS_CHAN_DEFINE(MQTT_CHAN,
				 struct velopera_payload,
				 NULL,
				 NULL,
				 ZBUS_OBSERVERS(transport),
				 ZBUS_MSG_INIT(0));

/* Define NETWORK_CHAN */
ZBUS_CHAN_DEFINE(NETWORK_CHAN,
				 enum network_status,
				 NULL,
				 NULL,
				 ZBUS_OBSERVERS(transport),
				 ZBUS_MSG_INIT(0));

/* Define FATAL_ERROR_CHAN */
ZBUS_CHAN_DEFINE(FATAL_ERROR_CHAN,
				 int,
				 NULL,
				 NULL,
				 ZBUS_OBSERVERS(error),
				 ZBUS_MSG_INIT(0));
