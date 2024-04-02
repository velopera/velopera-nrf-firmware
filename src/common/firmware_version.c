#include "velopera_nrf_firmware_config.h"
#include "firmware_version.h"

static const FirmwareVersion version = {
    .major = velopera_nrf_firmware_VERSION_MAJOR,
    .minor = velopera_nrf_firmware_VERSION_MINOR,
    .patch = velopera_nrf_firmware_VERSION_PATCH,
    .full = velopera_nrf_firmware_VERSION_FULL};

const FirmwareVersion *getFirmwareVersion()
{
    return &version;
}
