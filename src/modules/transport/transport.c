/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/smf.h>
#include <zephyr/net/tls_credentials.h>
#include "dynsec_mqtt_helper.h"
#include "message_channel.h"
#include <modem/modem_info.h>

#include "firmware_version.h"
extern char imei[16];

uint8_t login_topic[50] = "";

/* Register log module */
LOG_MODULE_REGISTER(transport, 4);

/* Register subscriber */
ZBUS_SUBSCRIBER_DEFINE(transport, CONFIG_MQTT_SAMPLE_TRANSPORT_MESSAGE_QUEUE_SIZE);

/* ID for subscribe topic - Used to verify that a subscription succeeded in on_mqtt_suback(). */
#define SUBSCRIBE_TOPIC_ID 2469

/* Forward declarations */
static const struct smf_state state[];
static void connect_work_fn(struct k_work *work);
static void mqtt_pub_work_fn(struct k_work *work);

/* Define connection work - Used to handle reconnection attempts to the MQTT broker */
static K_WORK_DELAYABLE_DEFINE(connect_work, connect_work_fn);
static K_WORK_DELAYABLE_DEFINE(mqtt_pub_work, mqtt_pub_work_fn);

K_MSGQ_DEFINE(gps_data_queue, sizeof(struct velopera_gps_data), 20, 4);
K_MSGQ_DEFINE(sensor_data_queue, sizeof(struct velopera_payload), 20, 4);

/* Define stack_area of application workqueue */
K_THREAD_STACK_DEFINE(stack_area, CONFIG_MQTT_SAMPLE_TRANSPORT_WORKQUEUE_STACK_SIZE);

/* Declare application workqueue. This workqueue is used to call dynsec_mqtt_helper_connect(), and
 * schedule reconnectionn attempts upon network loss or disconnection from MQTT.
 */
static struct k_work_q transport_queue;

struct velopera_payload login_msg;
struct velopera_gps_data gps_data;

/* Internal states */
enum module_state
{
	MQTT_CONNECTED,
	MQTT_DISCONNECTED
};

static uint8_t pub_topic[CONFIG_MQTT_SAMPLE_TRANSPORT_CLIENT_ID_BUFFER_SIZE + sizeof(CONFIG_MQTT_SAMPLE_TRANSPORT_PUBLISH_TOPIC)];
static uint8_t gps_pub_topic[CONFIG_MQTT_SAMPLE_TRANSPORT_CLIENT_ID_BUFFER_SIZE + sizeof(CONFIG_MQTT_SAMPLE_TRANSPORT_SUBSCRIBE_TOPIC)];

static uint8_t fota_sub_topic[CONFIG_MQTT_SAMPLE_TRANSPORT_CLIENT_ID_BUFFER_SIZE + sizeof(CONFIG_MQTT_SAMPLE_TRANSPORT_SUBSCRIBE_TOPIC)];
static uint8_t psk_sub_topic[CONFIG_MQTT_SAMPLE_TRANSPORT_CLIENT_ID_BUFFER_SIZE + sizeof(CONFIG_MQTT_SAMPLE_TRANSPORT_SUBSCRIBE_TOPIC)];

/* User defined state object.
 * Used to transfer data between state changes.
 */
static struct s_object
{
	/* This must be first */
	struct smf_ctx ctx;

	/* Last channel type that a message was received on */
	const struct zbus_channel *chan;

	/* Network status */
	enum network_status status;

	/* Payload */
	struct velopera_payload payload;

	/* Topic */
	uint8_t *topic;
} s_obj;

/**
 * @brief This helper function publishes an MQTT message to the broker
 *
 * @param payload  message that wanted to publish
 * @param topic topic of the published message
 * @param topic_size size of the topic
 */
static void publish(struct velopera_payload *payload, uint8_t *topic, size_t topic_size)
{
	int err;

	struct mqtt_publish_param param = {
		.message.payload.data = payload->string,
		.message.payload.len = strlen(payload->string),
		.message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE,
		.message_id = k_uptime_get_32(),
		.message.topic.topic.utf8 = topic,
		.message.topic.topic.size = strlen(topic),
	};

	err = dynsec_mqtt_helper_publish(&param);
	if (err)
	{
		LOG_WRN("Failed to send payload, err: %d", err);
		return;
	}

	LOG_DBG("Published message: \"%.*s\" on topic: \"%.*s\"", param.message.payload.len,
			param.message.payload.data,
			param.message.topic.topic.size,
			param.message.topic.topic.utf8);
}

static int modify_login_info_msg(char *msg, size_t msg_size)
{

	struct modem_param_info modem_param;

	int err = modem_info_init();
	if (err)
	{
		LOG_ERR("Failed to initialize modem info: %d", err);
	}

	err = modem_info_params_init(&modem_param);
	if (err)
	{
		LOG_ERR("Failed to initialize modem info: %d", err);
	}

	err = modem_info_params_get(&modem_param);
	if (err)
	{
		LOG_ERR("Failed to initialize modem info: %d", err);
	}

	LOG_DBG("====== modify_login_info_msg ======");
	err = snprintf(msg, msg_size, "{\"networkStatus\":\"online\",\"rsrp\":%d,\"iccid\":\"%s\",\"mcc\":\"%x\",\"mnc\":\"%s\",\"cid\":\"%s\",\"band\":\"%d\",\"areaCode\":\"%s\",\"op\":\"%s\",\"modem\":\"%s\",\"fw\":\"%s\"}",
				   modem_param.network.rsrp.value, modem_param.sim.iccid.value_string,
				   modem_param.network.mcc.value, modem_param.network.mnc.value_string,
				   modem_param.network.cellid_hex.value_string, modem_param.network.current_band.value,
				   modem_param.network.area_code.value_string, modem_param.network.current_operator.value_string,
				   modem_param.device.modem_fw.value_string, getFirmwareVersion()->full);

	if (err < 0)
	{
		LOG_ERR("snprintf %d", err);
	}

	// LOG_INF("IP Address: %s", modem_param.network.ip_address.value_string);

	LOG_DBG("msg (JSON): %s, size %d", msg, err);
	LOG_DBG("===============================");
	return err;
}

/* Callback handlers from MQTT helper library.
 * The functions are called whenever specific MQTT packets are received from the broker, or
 * some library state has changed.
 */
static void on_mqtt_connack(enum mqtt_conn_return_code return_code)
{
	ARG_UNUSED(return_code);

	smf_set_state(SMF_CTX(&s_obj), &state[MQTT_CONNECTED]);
}

static void on_mqtt_disconnect(int result)
{
	ARG_UNUSED(result);

	smf_set_state(SMF_CTX(&s_obj), &state[MQTT_DISCONNECTED]);
}

static void on_mqtt_publish(struct dynsec_mqtt_helper_buf topic, struct dynsec_mqtt_helper_buf payload)
{
	LOG_INF("Received payload: %.*s on topic: %.*s", payload.size,
			payload.ptr,
			topic.size,
			topic.ptr);

	if (strncmp(topic.ptr, fota_sub_topic, sizeof(fota_sub_topic)) == 0)
	{
		LOG_DBG("FOTA request received for firmware %s", payload.ptr);

		int err = zbus_chan_pub(&FOTA_CHAN, &payload, K_SECONDS(1));
		if (err)
		{
			LOG_ERR("zbus_chan_pub, error: %d", err);
			SEND_FATAL_ERROR();
		}

		LOG_DBG("FOTA request redirected to FOTA_CHAN");
	}
}

static void on_mqtt_suback(uint16_t message_id, int result)
{
	if ((message_id == SUBSCRIBE_TOPIC_ID) && (result == 0))
	{
		LOG_INF("Subscribed to topic %s and topic %s", fota_sub_topic, psk_sub_topic);
	}
	else if (result)
	{
		LOG_ERR("Topic subscription failed, error: %d", result);
	}
	else
	{
		LOG_WRN("Subscribed to unknown topic, id: %d", message_id);
	}
}

/* Local convenience functions */

/* Function that prefixes topics with the Client ID. */
static int topics_prefix(void)
{
	int len;

	len = snprintk(pub_topic, sizeof(pub_topic), "ind/%s/%s", imei,
				   CONFIG_MQTT_SAMPLE_TRANSPORT_PUBLISH_TOPIC);
	if ((len < 0) || (len >= sizeof(pub_topic)))
	{
		LOG_ERR("Publish topic buffer too small");
		return -EMSGSIZE;
	}

	len = snprintk(gps_pub_topic, sizeof(gps_pub_topic), "ind/%s/gps", imei);
	if ((len < 0) || (len >= sizeof(pub_topic)))
	{
		LOG_ERR("Publish topic buffer too small");
		return -EMSGSIZE;
	}

	len = snprintk(fota_sub_topic, sizeof(fota_sub_topic), "cmd/%s/%s", imei,
				   "fota");

	if ((len < 0) || (len >= sizeof(fota_sub_topic)))
	{
		LOG_ERR("Subscribe topic buffer too small %d", __LINE__);
		return -EMSGSIZE;
	}
	len = snprintk(psk_sub_topic, sizeof(psk_sub_topic), "cmd/%s/%s", imei,
				   "psk");

	if ((len < 0) || (len >= sizeof(psk_sub_topic)))
	{
		LOG_ERR("Subscribe topic buffer too small %d", __LINE__);
		return -EMSGSIZE;
	}

	return 0;
}

static void subscribe(void)
{
	int err;
	struct mqtt_topic topics[] = {
		{
			.topic.utf8 = fota_sub_topic,
			.topic.size = strlen(fota_sub_topic),
		},
		{
			.topic.utf8 = psk_sub_topic,
			.topic.size = strlen(psk_sub_topic),
		},
	};
	struct mqtt_subscription_list list = {
		.list = topics,
		.list_count = ARRAY_SIZE(topics),
		.message_id = SUBSCRIBE_TOPIC_ID,
	};

	for (size_t i = 0; i < list.list_count; i++)
	{
		LOG_INF("Subscribing to: %s", (char *)list.list[i].topic.utf8);
	}

	err = dynsec_mqtt_helper_subscribe(&list);
	if (err)
	{
		LOG_ERR("Failed to subscribe to topics, error: %d", err);
		return;
	}
}

/* Connect work - Used to establish a connection to the MQTT broker and schedule reconnection
 * attempts.
 */
static void connect_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);

	int err;
	struct dynsec_mqtt_helper_conn_params conn_params = {
		.hostname.ptr = CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_HOSTNAME,
		.hostname.size = strlen(CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_HOSTNAME),
		.user_name.ptr = imei,
		.user_name.size = strlen(imei),
		.device_id.ptr = imei,
		.device_id.size = strlen(imei),
		.password.ptr = imei, // TODO: for now!
		.password.size = strlen(imei),
		.last_will_message.ptr = "{\"networkStatus\":\"offline\"}",
		.last_will_message.size = strlen("{\"networkStatus\":\"offline\"}"),
	};
	err = snprintf(login_topic, sizeof(login_topic), "ind/%s/login", imei);
	err = topics_prefix();
	if (err)
	{
		LOG_ERR("topics_prefix, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
	conn_params.last_will_topic.ptr = login_topic;
	conn_params.last_will_topic.size = strlen(login_topic);
	err = dynsec_mqtt_helper_connect(&conn_params);
	if (err)
	{
		LOG_ERR("Failed connecting to MQTT, error code: %d", err);
	}

	k_work_reschedule_for_queue(&transport_queue, &connect_work,
								K_SECONDS(CONFIG_MQTT_SAMPLE_TRANSPORT_RECONNECTION_TIMEOUT_SECONDS));
}
void mqtt_pub_work_fn(struct k_work *work)
{
	struct velopera_gps_data gps_data;
	struct velopera_payload payload;
	int err;
	printf("%d \r\n", __LINE__);
	while (k_msgq_get(&gps_data_queue, &gps_data, K_NO_WAIT) == 0)
	{
		sprintf(payload.string, GNSS_DATA_JSON,
				gps_data.pvt.latitude,
				gps_data.pvt.longitude,
				gps_data.pvt.altitude,
				gps_data.pvt.accuracy,
				gps_data.pvt.speed,
				gps_data.pvt.speed_accuracy,
				gps_data.pvt.heading,
				gps_data.pvt.datetime.year,
				gps_data.pvt.datetime.month,
				gps_data.pvt.datetime.day,
				gps_data.pvt.datetime.hour,
				gps_data.pvt.datetime.minute,
				gps_data.pvt.datetime.seconds,
				gps_data.pvt.datetime.ms,
				gps_data.pvt.pdop,
				gps_data.pvt.hdop,
				gps_data.pvt.vdop,
				gps_data.pvt.tdop,
				gps_data.meas_id);

		printf("%s\n", payload.string);

		publish(&payload, gps_pub_topic, sizeof(gps_pub_topic));
		// int err = smf_run_state(SMF_CTX(&s_obj));
		// if (err)
		// {
		// 	LOG_ERR("smf_run_state, error: %d", err);
		// 	SEND_FATAL_ERROR();
		// 	return;
		// }
	}
	while (k_msgq_get(&sensor_data_queue, &payload, K_NO_WAIT) == 0)
	{

		printf("%s\n", payload.string);

		s_obj.payload = payload;
		s_obj.topic = pub_topic;
		publish(&payload, pub_topic, sizeof(pub_topic));
		// err = smf_run_state(SMF_CTX(&s_obj));
		// if (err)
		// {
		// 	LOG_ERR("smf_run_state, error: %d", err);
		// 	SEND_FATAL_ERROR();
		// 	return;
		// }
	}
}
/* Zephyr State Machine framework handlers */

/* Function executed when the module enters the disconnected state. */
static void disconnected_entry(void *o)
{
	struct s_object *user_object = o;

	/* Reschedule a connection attempt if we are connected to network and we enter the
	 * disconnected state.
	 */
	if (user_object->status == NETWORK_CONNECTED)
	{
		k_work_reschedule_for_queue(&transport_queue, &connect_work, K_NO_WAIT);
	}
}

/* Function executed when the module is in the disconnected state. */
static void disconnected_run(void *o)
{
	struct s_object *user_object = o;

	if ((user_object->status == NETWORK_DISCONNECTED) && (user_object->chan == &NETWORK_CHAN))
	{
		/* If NETWORK_DISCONNECTED is received after the MQTT connection is closed,
		 * we cancel the connect work if it is onging.
		 */
		k_work_cancel_delayable(&connect_work);
		k_work_cancel_delayable(&mqtt_pub_work);
	}

	if ((user_object->status == NETWORK_CONNECTED) && (user_object->chan == &NETWORK_CHAN))
	{

		/* Wait for 5 seconds to ensure that the network stack is ready before
		 * attempting to connect to MQTT. This delay is only needed when building for
		 * Wi-Fi.
		 */
		k_work_reschedule_for_queue(&transport_queue, &connect_work, K_SECONDS(5));
	}
}

/* Function executed when the module enters the connected state. */
static void connected_entry(void *o)
{
	LOG_INF("Connected to MQTT broker");
	LOG_INF("Hostname: %s", CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_HOSTNAME);
	LOG_INF("Client ID: %s", imei);
	LOG_INF("Port: %d", CONFIG_DYNSEC_MQTT_HELPER_PORT);
	LOG_INF("TLS: %s", IS_ENABLED(CONFIG_MQTT_LIB_TLS) ? "Yes" : "No");

	ARG_UNUSED(o);

	/* Cancel any ongoing connect work when we enter connected state */
	k_work_cancel_delayable(&connect_work);

	publish(&login_msg, login_topic, 50);

	subscribe();
	printf("LINE %d\r\n", __LINE__);

	k_work_submit_to_queue(&transport_queue, &mqtt_pub_work);
	printf("LINE %d\r\n", __LINE__);
}

/* Function executed when the module is in the connected state. */
static void connected_run(void *o)
{
	struct s_object *user_object = o;

	if ((user_object->status == NETWORK_DISCONNECTED) && (user_object->chan == &NETWORK_CHAN))
	{
		/* Explicitly disconnect the MQTT transport when losing network connectivity.
		 * This is to cleanup any internal library state.
		 * The call to this function will cause on_mqtt_disconnect() to be called.
		 */
		(void)dynsec_mqtt_helper_disconnect();
		return;
	}
	k_work_submit_to_queue(&transport_queue, &mqtt_pub_work);

	if (user_object->chan != &MQTT_CHAN)
	{
		return;
	}

	publish(&user_object->payload, user_object->topic, sizeof(user_object->topic));
}

/* Function executed when the module exits the connected state. */
static void connected_exit(void *o)
{
	ARG_UNUSED(o);

	LOG_INF("Disconnected from MQTT broker");
}

/* Construct state table */
static const struct smf_state state[] = {
	[MQTT_DISCONNECTED] = SMF_CREATE_STATE(disconnected_entry, disconnected_run, NULL),
	[MQTT_CONNECTED] = SMF_CREATE_STATE(connected_entry, connected_run, connected_exit),
};

static void transport_task(void)
{
	int err;
	static bool certs_added;
	LOG_INF("Transport task startup LINE %d", __LINE__);

	const struct zbus_channel *chan;
	enum network_status status;
	struct velopera_payload payload;
	struct dynsec_mqtt_helper_cfg cfg = {
		.cb = {
			.on_connack = on_mqtt_connack,
			.on_disconnect = on_mqtt_disconnect,
			.on_publish = on_mqtt_publish,
			.on_suback = on_mqtt_suback,
		},
	};

	s_obj.topic = pub_topic;

	/* Initialize and start application workqueue.
	 * This workqueue can be used to offload tasks and/or as a timer when wanting to
	 * schedule functionality using the 'k_work' API.
	 */
	k_work_queue_init(&transport_queue);
	k_work_queue_start(&transport_queue, stack_area,
					   K_THREAD_STACK_SIZEOF(stack_area),
					   K_HIGHEST_APPLICATION_THREAD_PRIO,
					   NULL);

	err = dynsec_mqtt_helper_init(&cfg);
	if (err)
	{
		LOG_ERR("dynsec_mqtt_helper_init, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	/* Set initial state */
	smf_set_initial(SMF_CTX(&s_obj), &state[MQTT_DISCONNECTED]);

	while (!zbus_sub_wait(&transport, &chan, K_FOREVER))
	{

		s_obj.chan = chan;

		if (&NETWORK_CHAN == chan)
		{

			err = zbus_chan_read(&NETWORK_CHAN, &status, K_SECONDS(1));
			if (err)
			{
				LOG_ERR("zbus_chan_read, error: %d", err);
				SEND_FATAL_ERROR();
				return;
			}

			err = modify_login_info_msg(login_msg.string, sizeof(login_msg.string));

			s_obj.status = status;

			err = smf_run_state(SMF_CTX(&s_obj));
			if (err)
			{
				LOG_ERR("smf_run_state, error: %d", err);
				SEND_FATAL_ERROR();
				return;
			}
		}

		if (&MQTT_CHAN == chan)
		{

			err = zbus_chan_read(&MQTT_CHAN, &payload, K_SECONDS(1));
			if (err)
			{
				LOG_ERR("zbus_chan_read, error: %d", err);
				SEND_FATAL_ERROR();
				return;
			}

			if (k_msgq_put(&sensor_data_queue, &payload, K_NO_WAIT) != 0)
			{
				LOG_WRN("Queue is full, could not add sensor data.\n");
			}

			// s_obj.payload = payload;
			// s_obj.topic = pub_topic;

			// err = smf_run_state(SMF_CTX(&s_obj));
			// if (err)
			// {
			// 	LOG_ERR("smf_run_state, error: %d", err);
			// 	SEND_FATAL_ERROR();
			// 	return;
			// }
		}
		if (&GPS_CHAN == chan)
		{
			printf("LINE %d\r\n", __LINE__);
			err = zbus_chan_read(&GPS_CHAN, &gps_data, K_SECONDS(1));
			if (err)
			{
				LOG_ERR("zbus_chan_read, error: %d", err);
				SEND_FATAL_ERROR();
				return;
			}
			printf("gps_data %d\r\n", gps_data.meas_id);
			if (k_msgq_put(&gps_data_queue, &gps_data, K_NO_WAIT) != 0)
			{
				LOG_WRN("Queue is full, could not add GPS data.\n");
			}
			printf("LINE %d\r\n", __LINE__); // s_obj.payload = payload;
											 // s_obj.topic = gps_pub_topic;

			// err = smf_run_state(SMF_CTX(&s_obj));
			// if (err)
			// {
			// 	LOG_ERR("smf_run_state, error: %d", err);
			// 	SEND_FATAL_ERROR();
			// 	return;
			// }
		}
	}
}

K_THREAD_DEFINE(transport_task_id,
				4096,
				transport_task, NULL, NULL, NULL, 3, 0, 0);
