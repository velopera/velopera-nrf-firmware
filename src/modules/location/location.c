/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <nrf_modem_at.h>
#include <modem/lte_lc.h>
#include <modem/nrf_modem_lib.h>
#include <nrf_modem_gnss.h>

#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(location_app, 4);

extern struct k_sem lte_connected;
K_SEM_DEFINE(gnss_fix_sem, 0, 1);
K_SEM_DEFINE(gnss_start_sem, 0, 1);

struct velopera_gps_data velo_gps_data;
bool gnss_active;
static void print_fix_data(struct velopera_gps_data *pvt_data)
{
	// LOG_INF("Latitude:       %.06f\n", pvt_data->latitude);
	// LOG_INF("Longitude:      %.06f\n", pvt_data->longitude);
	// LOG_INF("Altitude:       %.01f m\n", pvt_data->altitude);
	// LOG_INF("Accuracy:       %.01f m\n", pvt_data->accuracy);
	// LOG_INF("Speed:          %.01f m/s\n", pvt_data->speed);
	// LOG_INF("Speed accuracy: %.01f m/s\n", pvt_data->speed_accuracy);
	// LOG_INF("Heading:        %.01f deg\n", pvt_data->heading);
	// LOG_INF("Date:           %04u-%02u-%02u\n",
	// 		pvt_data->datetime.year,
	// 		pvt_data->datetime.month,
	// 		pvt_data->datetime.day);
	// LOG_INF("Time (CET):     %02u:%02u:%02u.%03u\n",
	// 		pvt_data->datetime.hour + 2,
	// 		pvt_data->datetime.minute,
	// 		pvt_data->datetime.seconds,
	// 		pvt_data->datetime.ms);
	// LOG_INF("PDOP:           %.01f\n", pvt_data->pdop);
	// LOG_INF("HDOP:           %.01f\n", pvt_data->hdop);
	// LOG_INF("VDOP:           %.01f\n", pvt_data->vdop);
	// LOG_INF("TDOP:           %.01f\n", pvt_data->tdop);
	LOG_INF("  Google maps URL: https://maps.google.com/?q=%.06f,%.06f\n\n",
			pvt_data->pvt.latitude, pvt_data->pvt.longitude);
}
static void print_satellite_stats(struct nrf_modem_gnss_pvt_data_frame *pvt_data)
{
	uint8_t tracked = 0;
	uint8_t in_fix = 0;
	uint8_t unhealthy = 0;

	for (int i = 0; i < NRF_MODEM_GNSS_MAX_SATELLITES; ++i)
	{
		if (pvt_data->sv[i].sv > 0)
		{
			tracked++;

			if (pvt_data->sv[i].flags & NRF_MODEM_GNSS_SV_FLAG_USED_IN_FIX)
			{
				in_fix++;
			}

			if (pvt_data->sv[i].flags & NRF_MODEM_GNSS_SV_FLAG_UNHEALTHY)
			{
				unhealthy++;
			}
		}
	}

	LOG_INF("Tracking: %2d Using: %2d Unhealthy: %d\n", tracked, in_fix, unhealthy);
}
static void gnss_event_handler(int event)
{
	int retval;
	switch (event)
	{
	case NRF_MODEM_GNSS_EVT_PVT:
		nrf_modem_gnss_read(&velo_gps_data.pvt, sizeof(velo_gps_data.pvt), NRF_MODEM_GNSS_DATA_PVT);
		LOG_INF("Searching for GNSS Satellites....\n\r");
		print_satellite_stats(&velo_gps_data.pvt);

		break;
	case NRF_MODEM_GNSS_EVT_FIX:
		LOG_INF("GNSS fix event\n\r");
		retval = nrf_modem_gnss_read(&velo_gps_data.pvt, sizeof(velo_gps_data.pvt), NRF_MODEM_GNSS_DATA_PVT);
		if (retval == 0)
		{
			// current_pvt[meas_id] = velo_gps_data.pvt;
			print_fix_data(&velo_gps_data);
			k_sem_give(&gnss_fix_sem);
		}
		break;
	case NRF_MODEM_GNSS_EVT_PERIODIC_WAKEUP:
		velo_gps_data.meas_id = 0;
		LOG_INF("GNSS woke up in periodic mode\n\r");
		break;
	case NRF_MODEM_GNSS_EVT_BLOCKED:
		LOG_INF("GNSS is blocked by LTE event\n\r");
		break;
	case NRF_MODEM_GNSS_EVT_SLEEP_AFTER_FIX:
		LOG_INF("GNSS enters sleep because fix was achieved in periodic mode\n\r");

		break;
	case NRF_MODEM_GNSS_EVT_SLEEP_AFTER_TIMEOUT:
		LOG_INF("GNSS enters sleep because fix retry timeout was reached\n\r");
		break;
	default:

		break;
	}
}

static int gnss_init_and_start(void)
{
#if defined(CONFIG_GNSS_HIGH_ACCURACY_TIMING_SOURCE)
	if (nrf_modem_gnss_timing_source_set(NRF_MODEM_GNSS_TIMING_SOURCE_TCXO))
	{
		LOG_ERR("Failed to set TCXO timing source");
		return -1;
	}
#endif
	uint8_t use_case;
	use_case = NRF_MODEM_GNSS_USE_CASE_MULTIPLE_HOT_START | NRF_MODEM_GNSS_USE_CASE_LOW_ACCURACY;
	if (nrf_modem_gnss_use_case_set(use_case) != 0)
	{
		LOG_ERR("Failed to set low accuracy use case");
		return -1;
	}
	/* Configure GNSS event handler . */
	if (nrf_modem_gnss_event_handler_set(gnss_event_handler) != 0)
	{
		LOG_ERR("Failed to set GNSS event handler");
		return -1;
	}

	if (nrf_modem_gnss_fix_interval_set(1) != 0)
	{
		LOG_ERR("Failed to set GNSS fix interval");
		return -1;
	}

	if (nrf_modem_gnss_fix_retry_set(60) != 0)
	{
		LOG_ERR("Failed to set GNSS fix retry");
		return -1;
	}

	if (nrf_modem_gnss_start() != 0)
	{
		LOG_ERR("Failed to start GNSS");
		return -1;
	}
	if (nrf_modem_gnss_prio_mode_enable() != 0)
	{
		LOG_ERR("Error setting GNSS priority mode");
		return -1;
	}
	gnss_active = true;
	return 0;
}
static void stop_gnss(void)
{
	if (nrf_modem_gnss_stop() != 0)
	{
		LOG_ERR("Failed to stop GNSS");
	}
}

static void location_task(void)
{
	int err;

	/* Wait untill first LTE connection */
	while (k_sem_take(&lte_connected, K_FOREVER))
	{
	}
	LOG_INF("Deactivating LTE for first GNSS fix");

	gnss_init_and_start();
	struct velopera_gps_data velo_gps_test_data;

	while (true)
	{

		while (gnss_active)
		{
			k_sem_take(&gnss_fix_sem, K_FOREVER);
			zbus_chan_pub(&GPS_CHAN, &velo_gps_data, K_SECONDS(10));
			velo_gps_data.meas_id++;
			LOG_INF("gps data published to be added in queue");
		}
		// k_sleep(K_SECONDS(60));
		k_sem_take(&lte_connected, K_FOREVER);
		LOG_INF("GNSS was active for %d seconds", 60);

		stop_gnss();

		k_sleep(K_SECONDS(60));

		k_sem_give(&gnss_start_sem);
		LOG_INF("Reactivating GNSS");

		gnss_init_and_start();

		// for (int i = 0; i < 4; i++)
		// {
		// 	printf("LINE %d\r\n", __LINE__);
		// 	velo_gps_test_data.pvt.latitude += 2;
		// 	velo_gps_test_data.pvt.longitude += 2;
		// 	velo_gps_test_data.pvt.altitude += 2;
		// 	velo_gps_test_data.pvt.accuracy += 2;
		// 	velo_gps_test_data.pvt.speed += 2;
		// 	velo_gps_test_data.pvt.speed_accuracy += 2;
		// 	velo_gps_test_data.pvt.heading += 2;
		// 	velo_gps_test_data.pvt.datetime.year += 2;
		// 	velo_gps_test_data.pvt.datetime.month += 2;
		// 	velo_gps_test_data.pvt.datetime.day += 2;
		// 	velo_gps_test_data.pvt.datetime.hour += 2;
		// 	velo_gps_test_data.pvt.datetime.minute += 2;
		// 	velo_gps_test_data.pvt.datetime.seconds += 2;
		// 	velo_gps_test_data.pvt.datetime.ms += 2;
		// 	velo_gps_test_data.pvt.pdop += 2;
		// 	velo_gps_test_data.pvt.hdop += 2;
		// 	velo_gps_test_data.pvt.vdop += 2;
		// 	velo_gps_test_data.pvt.tdop += 2;
		// 	velo_gps_test_data.meas_id += 1;
		// 	zbus_chan_pub(&GPS_CHAN, &velo_gps_test_data, K_SECONDS(10));
		// 	printf("LINE %d\r\n", __LINE__);

		// 	// printfk_sleep(K_MSEC(2000));
		// 	printf("LINE %d\r\n", __LINE__);
		// }
		// k_sleep(K_SECONDS(20));
	}
}

K_THREAD_DEFINE(location_task_id,
				4096,
				location_task, NULL, NULL, NULL, 3, 0, 0);
