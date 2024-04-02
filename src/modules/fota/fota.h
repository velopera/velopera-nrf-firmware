#ifndef UPDATE_H__
#define UPDATE_H__

#define TLS_SEC_TAG 42

#ifndef CONFIG_USE_HTTPS
#define SEC_TAG (-1)
#else
#define SEC_TAG (TLS_SEC_TAG)
#endif

/**
 * @brief This function initializes the FOTA
 *
 */
int fota_init();

/**
 * @brief This function concatenates the filename global variable with the directory
 * that contains the firmware and the firmware name and submits fota_work to the system
 * queue.
 *
 * @param file
 * @param filename_size
 */
void fota_request(const char *file, size_t filename_size);

/**
 * @brief Notify the library that the update has been completed.
 * This will send the modem to power off mode, while waiting
 * for a reset.
 *
 */
void fota_done(void);

#endif /* UPDATE_H__ */