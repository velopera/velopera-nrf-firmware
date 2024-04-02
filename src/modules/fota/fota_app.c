#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/zbus/zbus.h>

#include "fota.h"
#include "message_channel.h"

LOG_MODULE_REGISTER(fota_app, 4);

ZBUS_SUBSCRIBER_DEFINE(fota_app, 5);

void fota_task(void)
{
    int err;
    const struct zbus_channel *chan;
    struct fota_filename filename;

    LOG_INF("------------FIRMWARE DOWNGRADED VIA FOTA-------------");

    /* Initiliazes FOTA */
    err = fota_init();
    if (err)
    {
        LOG_ERR("fota_init failed with err %d", err);
    }

    /* Polls zbus message notification */
    while (!zbus_sub_wait(&fota_app, &chan, K_FOREVER))
    {

        /* Checks if notification comes from FOTA_CHAN */
        if (&FOTA_CHAN == chan)
        {
            /* Reads the message context (firmware filename) */
            err = zbus_chan_read(&FOTA_CHAN, &filename, K_SECONDS(1));
            if (err)
            {
                LOG_ERR("zbus_chan_read, error: %d", err);
                SEND_FATAL_ERROR();
                return;
            }

            k_msleep(10);

            /* Requests the update for corresponding filename*/
            fota_request(filename.ptr, filename.size);
        }
    }
}

/* Defines the thread for fota_task */
K_THREAD_DEFINE(fota_task_id,
                4096,
                fota_task, NULL, NULL, NULL, 3, 0, 0);