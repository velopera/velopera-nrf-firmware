#ifndef FIRMWARE_VERSION_H
#define FIRMWARE_VERSION_H

typedef struct FirmwareVersion
{
    int major;
    int minor;
    int patch;
    const char *full;
} FirmwareVersion;

extern const FirmwareVersion *getFirmwareVersion();

#endif // FIRMWARE_VERSION_H