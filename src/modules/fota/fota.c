#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/toolchain.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>

#include <modem/lte_lc.h>
#include <net/fota_download.h>
#include <dfu/dfu_target_mcuboot.h>

#include "fota.h"

LOG_MODULE_REGISTER(update, 4);

static struct k_work fota_work;

static char filename[128] = {0};

/**
 * @brief Handler for the generated FOTA events
 *
 * @param evt
 */
static void fota_dl_handler(const struct fota_download_evt *evt)
{
    switch (evt->id)
    {
    case FOTA_DOWNLOAD_EVT_ERROR:
        printk("Received error from fota_download\n");
        break;

    case FOTA_DOWNLOAD_EVT_FINISHED:
        fota_done();
        printk("Press 'Reset' button or enter 'reset' to apply new firmware\n");
        break;

    default:
        break;
    }
}

/**
 * @brief This function starts FOTA with hostname which defined by CONFIG_DOWNLOAD_HOST
 * and filename global variable.
 */
static void fota_start(void)
{
    int err;
    LOG_DBG("FOTA starting");

    /* Functions for getting the host and file */
    err = fota_download_start(CONFIG_DOWNLOAD_HOST, filename, SEC_TAG, 0, 0);
    if (err != 0)
    {

        LOG_ERR("fota_download_start() failed, err %d\n", err);
    }
}

/**
 * @brief This callback function starts fota once
 * it is triggered by fota_request function
 *
 * @param work
 */
static void fota_work_cb(struct k_work *work)
{
    ARG_UNUSED(work);

    LOG_DBG("fota_work_cb triggered");

    fota_start();
}

/**
 * @brief This callback function executes fota_start function
 *
 * @param shell
 * @param argc
 * @param argv
 * @return int
 */
static int shell_download(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    fota_start();

    return 0;
}

/**
 * @brief This callback function executes sys_reboot function for reboot
 *
 * @param shell
 * @param argc
 * @param argv
 * @return int
 */
static int shell_reboot(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_print(shell, "Device will now reboot");
    sys_reboot(SYS_REBOOT_WARM);

    return 0;
}

/* Register shell_reboot callback function as root command */
SHELL_CMD_REGISTER(reset, NULL, "For rebooting device", shell_reboot);

/* Register shell_download callback function as root command */
SHELL_CMD_REGISTER(download, NULL, "For downloading modem  firmware", shell_download);

void fota_request(const char *file, size_t filename_size)
{

    strncpy(filename, CONFIG_DOWNLOAD_HOST, sizeof(filename));
    strncat(filename, "/files/", 8); //
    strncat(filename, file, filename_size);
    k_work_submit(&fota_work);
}

int fota_init()
{
    int err;

    /* This is needed so that MCUBoot won't revert the update */
    boot_write_img_confirmed();

    /* Initiliaze fota download library with fota_dl_handler */
    err = fota_download_init(fota_dl_handler);
    if (err != 0)
    {
        LOG_ERR("fota_download_init() failed, err %d\n", err);
        return err;
    }

    /* Initiliaze kernel work structure for FOTA with fota_work_cb */
    k_work_init(&fota_work, fota_work_cb);
    return 0;
}

void fota_done(void)
{
#if !defined(CONFIG_LWM2M_CARRIER)
    lte_lc_deinit();
#endif
}