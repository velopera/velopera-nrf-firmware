/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/net/conn_mgr_monitor.h>

#include <modem/modem_key_mgmt.h>
#include <modem/lte_lc.h>
#include <modem/pdn.h>
#include <nrf_modem_gnss.h>

#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(network, 4);
char response[64];
extern bool gnss_active;
extern struct k_sem gnss_fix_sem;
extern struct k_sem gnss_start_sem;
K_SEM_DEFINE(lte_connected, 0, 1);

/* This module does not subscribe to any channels */
/* Value that holds the latest LTE network mode. */
static enum lte_lc_lte_mode nw_mode_latest;

static void print_modem_status(void)
{
	char response[64];

	int err = nrf_modem_at_cmd(response,
							   sizeof(response), "AT+CPAS");

	if (err)
	{
		LOG_ERR("ERR nrf_modem_at_cmd device activity status %d \r\n", err);
	}

	LOG_DBG("Device activity status: %s \r\n", response);

	err = nrf_modem_at_cmd(response,
						   sizeof(response), "AT+CFUN?");

	if (err)
	{
		LOG_ERR("ERR nrf_modem_at_cmd device activity status %d \r\n", err);
	}

	LOG_DBG("Device activity status: %s \r\n", response);

	err = nrf_modem_at_cmd(response,
						   sizeof(response), "AT+CEMODE?");

	if (err)
	{
		LOG_ERR("ERR nrf_modem_at_cmd device mode %d \r\n", err);
	}

	LOG_INF("Device activity status: %s \r\n", response);

	err = nrf_modem_at_cmd(response,
						   sizeof(response), "AT%%XSYSTEMMODE?");

	if (err)
	{
		LOG_ERR("ERR nrf_modem_at_cmd device mode %d \r\n", err);
	}

	LOG_DBG("Device activity status: %s \r\n", response);
}

/* Handler that is used to notify the application about LTE link specific events. */
static void lte_event_handler(const struct lte_lc_evt *const evt)
{
	switch (evt->type)
	{
	case LTE_LC_EVT_NW_REG_STATUS:
	{
		if (evt->nw_reg_status == LTE_LC_NW_REG_UICC_FAIL)
		{
			LOG_ERR("No SIM card detected!");
			break;
		}

		if ((evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME) ||
			(evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_ROAMING))
		{
			LOG_DBG("Network registration status: %s",
					evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ? "Connected - home network" : "Connected - roaming");
			k_sem_give(&lte_connected);
		}

		break;
	}
	case LTE_LC_EVT_PSM_UPDATE:
		LOG_DBG("PSM parameter update: TAU: %d, Active time: %d",
				evt->psm_cfg.tau, evt->psm_cfg.active_time);

		break;
	case LTE_LC_EVT_EDRX_UPDATE:
	{
		char log_buf[60];
		long len;

		len = snprintf(log_buf, sizeof(log_buf),
					   "eDRX parameter update: eDRX: %.2f, PTW: %.2f",
					   evt->edrx_cfg.edrx, evt->edrx_cfg.ptw);
		if (len > 0)
		{
			LOG_DBG("%s", log_buf);
		}

		break;
	}
	case LTE_LC_EVT_RRC_UPDATE:
		LOG_DBG("RRC mode: %s",
				evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED ? "Connected" : "Idle");
		break;
	case LTE_LC_EVT_CELL_UPDATE:
		LOG_DBG("LTE cell changed: Cell ID: %d, Tracking area: %d",
				evt->cell.id, evt->cell.tac);
		break;
	case LTE_LC_EVT_LTE_MODE_UPDATE:
		nw_mode_latest = evt->lte_mode;
		break;
	case LTE_LC_EVT_MODEM_EVENT:
		LOG_DBG("Modem domain event, type: %s",
				evt->modem_evt == LTE_LC_MODEM_EVT_LIGHT_SEARCH_DONE ? "Light search done" : evt->modem_evt == LTE_LC_MODEM_EVT_SEARCH_DONE ? "Search done"
																						 : evt->modem_evt == LTE_LC_MODEM_EVT_RESET_LOOP	? "Reset loop"
																						 : evt->modem_evt == LTE_LC_MODEM_EVT_BATTERY_LOW	? "Low battery"
																						 : evt->modem_evt == LTE_LC_MODEM_EVT_OVERHEATED	? "Modem is overheated"
																																			: "Unknown");

		/* If a reset loop happens in the field, it should not be necessary
		 * to perform any action. The modem will try to re-attach to the LTE network after
		 * the 30-minute block.
		 */
		if (evt->modem_evt == LTE_LC_MODEM_EVT_RESET_LOOP)
		{
			LOG_WRN("The modem has detected a reset loop. LTE network attach is now "
					"restricted for the next 30 minutes. Power-cycle the device to "
					"circumvent this restriction.");
			LOG_DBG("For more information see the nRF91 AT Commands - Command "
					"Reference Guide v2.0 - chpt. 5.36");
		}
		break;
	default:
		break;
	}
}

/* Handler that notifies the application of events related to the default PDN context, CID 0. */
void pdn_event_handler(uint8_t cid, enum pdn_event event, int reason)
{
	ARG_UNUSED(cid);

	int err;
	enum network_status status;

	switch (event)
	{
	case PDN_EVENT_CNEC_ESM:
		LOG_DBG("Event: PDP context %d, %s", cid, pdn_esm_strerror(reason));
		return;
	case PDN_EVENT_ACTIVATED:
	{
		LOG_INF("PDN connection activated, IPv4 up");
		status = NETWORK_CONNECTED;

		break;
	}
	case PDN_EVENT_DEACTIVATED:
		LOG_INF("PDN connection deactivated");
		status = NETWORK_DISCONNECTED;
		break;
	case PDN_EVENT_IPV6_UP:
	{
		LOG_DBG("PDN_EVENT_IPV6_UP");

		return;
	}
	case PDN_EVENT_IPV6_DOWN:
		LOG_DBG("PDN_EVENT_IPV6_DOWN");
		return;
	default:
		LOG_ERR("Unexpected PDN event: %d", event);
		return;
	}

	err = zbus_chan_pub(&NETWORK_CHAN, &status, K_SECONDS(1));
	if (err)
	{
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
	}
}

static int start_lte(void)
{
	int err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_NORMAL);
	if (err)
	{
		LOG_ERR("Failed to activate LTE");
		return err;
	}
	return 0;
}

static int stop_lte(void)
{
	int err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_DEACTIVATE_LTE);
	if (err)
	{
		LOG_ERR("Failed to deactivate LTE");
		return err;
	}
	return 0;
}

static void network_task(void)
{
	/* Initialize LTE Link Control library*/
	int err = lte_lc_init();

	if (err)
	{
		LOG_ERR("lte_lc_init, error: %d", err);
		return err;
	}

	/* Setup a callback for the default PDP context. */
	err = pdn_default_ctx_cb_reg(pdn_event_handler);

	if (err)
	{
		LOG_ERR("pdn_default_ctx_cb_reg, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	/* Subscribe to modem domain events (AT%MDMEV).
	 * Modem domain events is received in the lte_event_handler().
	 *
	 * This function fails for modem firmware versions < 1.3.0 due to not being supported.
	 * Therefore we ignore its return value.
	 */

	(void)lte_lc_modem_events_enable();

	err = lte_lc_psm_req(true);

	if (err)
	{
		LOG_ERR("lte_lc_psm_req, error: %d", err);
		SEND_FATAL_ERROR();
	}

	print_modem_status();

	LOG_INF("Connecting to LTE...");

	/* Initialize the link controller and connect to LTE network.
	 * Register handler to receive LTE link specific events.
	 */
	err = lte_lc_connect_async(lte_event_handler);
	if (err)
	{
		LOG_ERR("lte_lc_init_and_connect, error: %d", err);
		SEND_FATAL_ERROR();
	}
	k_sem_give(&lte_connected);
	err = stop_lte();
	if (err)
	{
		LOG_ERR("Failed to deactivate LTE and enable GNSS functional mode");
		return;
	}
	while (1)
	{
		k_sem_take(&gnss_fix_sem, K_FOREVER);
		k_sleep(K_SECONDS(60));
		gnss_active = false;
		k_sem_give(&lte_connected);

		LOG_INF("Activating LTE for data transfer");
		err = start_lte();
		if (err)
		{
			LOG_ERR("Failed to activate LTE");
			return;
		}
		k_sem_take(&gnss_start_sem, K_FOREVER);
		err = stop_lte();
		if (err)
		{
			LOG_ERR("Failed to deactivate LTE");
			return;
		}
	}
	// k_sleep(K_SECONDS(300));
}

K_THREAD_DEFINE(network_task_id,
				4096,
				network_task, NULL, NULL, NULL, 3, 0, 0);
