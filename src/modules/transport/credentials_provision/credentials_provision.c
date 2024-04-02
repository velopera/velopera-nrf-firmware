/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <modem/modem_key_mgmt.h>
#include <modem/nrf_modem_lib.h>
#include "psk.h"
#include "../credentials/psk.h"

static unsigned char psk[] = PSK;
#define TLS_SEC_TAG 42

#ifndef CONFIG_USE_HTTPS
#define SEC_TAG (-1)
#else
#define SEC_TAG (TLS_SEC_TAG)
#endif
// static unsigned char psk_id[] = "c1";
char imei[16] = "";
static const unsigned char ca_certificate[] = {
#if __has_include("ca-cert.pem")
#include "ca-cert.pem"
#else
	""
#endif
};

static const unsigned char device_certificate[] = {
#if __has_include("client-cert.pem")
#include "client-cert.pem"
#else
	""
#endif
};

static const unsigned char private_key[] = {
#if __has_include("private-key.pem")
#include "private-key.pem"
#else
	""
#endif
};

#if CONFIG_DYNSEC_MQTT_HELPER_SECONDARY_SEC_TAG != -1

static const unsigned char ca_certificate_2[] = {
#if __has_include("ca-cert-2.pem")
#include "ca-cert-2.pem"
#else
	""
#endif
};

static const unsigned char private_key_2[] = {
#if __has_include("private-key-2.pem")
#include "private-key-2.pem"
#else
	""
#endif
};

static const unsigned char device_certificate_2[] = {
#if __has_include("client-cert-2.pem")
#include "client-cert-2.pem"
#else
	""
#endif
};

#endif /* CONFIG_DYNSEC_MQTT_HELPER_SECONDARY_SEC_TAG != -1 */

static const char cert[] = {
	"-----BEGIN CERTIFICATE-----\n"
	"RvTZ8M6UK+5UzhK8jCdLuMGYL6KvzXGRSgi3yLgjewQtCPkIVz6D2QQz\n"
	"CkcheAmCJ8MqyJu5zlzyZMjAvnnAT45tRAxekrsu94sQ4egdRCnbWSDtY7kh+BIm\n"
	"lJNXoB1lBMEKIq4QDUOXoRgffuDghje1WrG9ML+Hbisq/yFOGwXD9RiX8F6sw6W4\n"
	"avAuvDszue5L3sz85K+EC4Y/wFVDNvZo4TYXao6Z0f+lQKc0t8DQYzk1OXVu8rp2\n"
	"yJMC6alLbBfODALZvYH7n7do1AZls4I9d1P4jnkDrQoxB3UqQ9hVl3LEKQ73xF1O\n"
	"yK5GhDDX8oVfGKF5u+decIsH4YaTw7mP3GFxJSqv3+0lUFJoi5Lc5da149p90Ids\n"
	"hCExroL1+7mryIkXPeFM5TgO9r0rvZaBFOvV2z0gp35Z0+L4WPlbuEjN/lxPFin+\n"
	"HlUjr8gRsI3qfJOQFy/9rKIJR0Y/8Omwt/8oTWgy1mdeHmmjk7j1nYsvC9JSQ6Zv\n"
	"MldlTTKB3zhThV1+XWYp6rjd5JW1zbVWEkLNxE7GJThEUG3szgBVGP7pSWTUTsqX\n"
	"nLRbwHOoq7hHwg==\n"
	"-----END CERTIFICATE-----\n"

	// #include "../fota/lab_voxel_at_CA"
};
int fota_cert_provision(void)
{

	BUILD_ASSERT(sizeof(cert) < KB(4), "Certificate too large");

	int err;
	bool exists;

	err = modem_key_mgmt_exists(TLS_SEC_TAG,
								MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, &exists);
	if (err)
	{
		printk("Failed to check for certificates err %d\n", err);
		return err;
	}

	if (exists)
	{
		/* For the sake of simplicity we delete what is provisioned
		 * with our security tag and reprovision our certificate.
		 */
		err = modem_key_mgmt_delete(TLS_SEC_TAG,
									MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN);
		if (err)
		{
			printk("Failed to delete existing certificate, err %d\n",
				   err);
		}
	}

	printk("Provisioning certificate for FOTA \n");

	/*  Provision certificate to the modem */
	err = modem_key_mgmt_write(TLS_SEC_TAG,
							   MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, cert,
							   sizeof(cert) - 1);
	if (err)
	{
		printk("Failed to provision certificate, err %d, line %d\n", err, __LINE__);
		return err;
	}

	return 0;
}

static int credentials_provision(void)
{
	int err = 0;

	if (sizeof(ca_certificate) > 1)
	{
		err = modem_key_mgmt_write(CONFIG_DYNSEC_MQTT_HELPER_SEC_TAG,
								   MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN,
								   ca_certificate,
								   sizeof(ca_certificate) - 1);
		if (err)
		{
			return err;
		}
	}

	if (sizeof(device_certificate) > 1)
	{
		err = modem_key_mgmt_write(CONFIG_DYNSEC_MQTT_HELPER_SEC_TAG,
								   MODEM_KEY_MGMT_CRED_TYPE_PUBLIC_CERT,
								   device_certificate,
								   sizeof(device_certificate) - 1);
		if (err)
		{
			return err;
		}
	}

	if (sizeof(private_key) > 1)
	{
		err = modem_key_mgmt_write(CONFIG_DYNSEC_MQTT_HELPER_SEC_TAG,
								   MODEM_KEY_MGMT_CRED_TYPE_PRIVATE_CERT,
								   private_key,
								   sizeof(private_key) - 1);
		if (err)
		{
			return err;
		}
	}

	char response[64];

	/* Read IMEI to use it as PSK_ID */
	err = nrf_modem_at_cmd(response,
						   sizeof(response), "AT+CGSN");

	if (err)
	{
		printk("ERR nrf_modem_at_cmd imei %d \r\n", err);
	}

	response[15] = '\0';
	strncpy(imei, response, strlen(response));
	printk("IMEI: %s \r\n", imei);

	/* Write IMEI as PSK_ID to the credential storage */
	err = modem_key_mgmt_write(CONFIG_DYNSEC_MQTT_HELPER_SEC_TAG,
							   MODEM_KEY_MGMT_CRED_TYPE_IDENTITY,
							   imei,
							   strlen(imei));
	if (err)
	{
		printk("error modem_key_mgmt_write %d \r\n", err);
		return err;
	}

	/* Write PSK to the credential storage */
	err = modem_key_mgmt_write(CONFIG_DYNSEC_MQTT_HELPER_SEC_TAG,
							   MODEM_KEY_MGMT_CRED_TYPE_PSK,
							   psk,
							   strlen(psk));

	if (err)
	{
		printk("error modem_key_mgmt_write %d \r\n", err);
		return err;
	}
	printk("modem_key_write succesfully \r\n");

	fota_cert_provision();

	/* Secondary security tag entries. */

#if CONFIG_DYNSEC_MQTT_HELPER_SECONDARY_SEC_TAG != -1

	if (sizeof(ca_certificate_2) > 1)
	{
		err = modem_key_mgmt_write(CONFIG_DYNSEC_MQTT_HELPER_SECONDARY_SEC_TAG,
								   MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN,
								   ca_certificate_2,
								   sizeof(ca_certificate_2) - 1);
		if (err)
		{
			return err;
		}
	}

	if (sizeof(device_certificate_2) > 1)
	{
		err = modem_key_mgmt_write(CONFIG_DYNSEC_MQTT_HELPER_SECONDARY_SEC_TAG,
								   MODEM_KEY_MGMT_CRED_TYPE_PUBLIC_CERT,
								   device_certificate_2,
								   sizeof(device_certificate_2) - 1);
		if (err)
		{
			return err;
		}
	}

	if (sizeof(private_key_2) > 1)
	{
		err = modem_key_mgmt_write(CONFIG_DYNSEC_MQTT_HELPER_SECONDARY_SEC_TAG,
								   MODEM_KEY_MGMT_CRED_TYPE_PRIVATE_CERT,
								   private_key_2,
								   sizeof(private_key_2) - 1);
		if (err)
		{
			return err;
		}
	}

#endif /* CONFIG_DYNSEC_MQTT_HELPER_SECONDARY_SEC_TAG != -1 */

	return err;
}

NRF_MODEM_LIB_ON_INIT(mqtt_sample_init_hook, on_modem_lib_init, NULL);

static void on_modem_lib_init(int ret, void *ctx)
{
	credentials_provision();
}
