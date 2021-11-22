/*
 * Copyright (c) 2018 Particle Industries, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "deviceid_hal.h"
#include "exflash_hal.h"
#include "str_util.h"
#include "system_error.h"
#include "check.h"
#ifndef HAL_DEVICE_ID_NO_DCT
#include "dct.h"
#endif /* HAL_DEVICE_ID_NO_DCT */

#include <algorithm>
#include <memory>

extern "C" {
#include "rtl8721d.h"
#include "rtl8721d_efuse.h"
}

namespace {

using namespace particle;

#define DEVICE_ID_PREFIX    {0x0a, 0x10, 0xac, 0xed, 0x20, 0x21}

#define LOGICAL_EFUSE_SIZE      1024
#define EFUSE_SUCCESS           1
#define EFUSE_FAILURE           0

#define WIFI_MAC_OFFSET         0x11A
#define MOBILE_SECRET_OFFSET    0x160
#define SERIAL_NUMBER_OFFSET    0x16F
#define HARDWARE_DATA_OFFSET    0x178
#define HARDWARE_MODEL_OFFSET   0x17B

#define WIFI_MAC_SIZE                   6
#define SERIAL_NUMBER_FRONT_PART_SIZE   9
#define HARDWARE_DATA_SIZE              4
#define HARDWARE_MODEL_SIZE             4

int readLogicalEfuse(uint32_t offset, uint8_t* buf, size_t size) {
    std::unique_ptr<uint8_t[]> efuseBuf(new uint8_t[LOGICAL_EFUSE_SIZE]);
    CHECK_TRUE(efuseBuf, SYSTEM_ERROR_NO_MEMORY);
    memset(efuseBuf.get(), 0xFF, sizeof(efuseBuf));

    auto ret = EFUSE_LMAP_READ(efuseBuf.get());
    CHECK_TRUE(ret == EFUSE_SUCCESS, SYSTEM_ERROR_INTERNAL);

    memcpy(buf, efuseBuf.get() + offset, size);

    return SYSTEM_ERROR_NONE;
};

} // Anonymous

unsigned HAL_device_ID(uint8_t* dest, unsigned destLen)
{
    // Device ID is composed of prefix and MAC address
    uint8_t id[2][6] = { DEVICE_ID_PREFIX, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff} };
    static_assert(sizeof(id) == HAL_DEVICE_ID_SIZE, "");
    CHECK_RETURN(readLogicalEfuse(WIFI_MAC_OFFSET, &id[1][0], WIFI_MAC_SIZE), 0);
    if (dest && destLen > 0) {
        memcpy(dest, id, std::min(destLen, sizeof(id)));
    }
    return HAL_DEVICE_ID_SIZE;
}

unsigned HAL_Platform_ID()
{
    return PLATFORM_ID;
}

int HAL_Get_Device_Identifier(const char** name, char* buf, size_t buflen, unsigned index, void* reserved)
{
    return -1;
}

int hal_get_device_serial_number(char* str, size_t size, void* reserved)
{
    char serial[HAL_DEVICE_SERIAL_NUMBER_SIZE] = {};

    // Serial Number is composed of Product Code (4 bytes), Manufacturer ID (2 bytes), 
    // Year (1 byte), Week (2 bytes) and MAC address (6 bytes)
    // We saved the front part (9 bytes) in the logical efuse
    CHECK(readLogicalEfuse(SERIAL_NUMBER_OFFSET, (uint8_t*)serial, SERIAL_NUMBER_FRONT_PART_SIZE));
    CHECK(readLogicalEfuse(WIFI_MAC_OFFSET, (uint8_t*)&serial[SERIAL_NUMBER_FRONT_PART_SIZE], WIFI_MAC_SIZE));
    if (!isPrintable(serial, sizeof(serial))) {
        return SYSTEM_ERROR_INTERNAL;
    }
    if (str) {
        memcpy(str, serial, std::min(size, sizeof(serial)));
        // Ensure the output is null-terminated
        if (sizeof(serial) < size) {
            str[sizeof(serial)] = '\0';
        }
    }
    return HAL_DEVICE_SERIAL_NUMBER_SIZE;
}

int hal_get_device_hw_version(uint32_t* revision, void* reserved)
{
    // HW Data format: | NCP_ID | HW_VERSION | HW Feature Flags |
    //                 | byte 0 |   byte 1   |    byte 2/3      |
    uint8_t hw_data[4] = {};
    CHECK(readLogicalEfuse(HARDWARE_DATA_OFFSET, (uint8_t*)hw_data, HARDWARE_DATA_SIZE));
    if (hw_data[1] == 0xFF) {
        return SYSTEM_ERROR_BAD_DATA;
    }
    *revision = hw_data[1];
    return SYSTEM_ERROR_NONE;
}

int hal_get_device_hw_model(uint32_t* model, uint32_t* variant, void* reserved)
{
    // HW Model format: | Model Number LSB | Model Number MSB | Model Variant LSB | Model Variant MSB |
    //                  |      byte 0      |      byte 1      |      byte 2       |      byte 3       |
    uint8_t hw_model[4] = {};
    CHECK(readLogicalEfuse(HARDWARE_MODEL_OFFSET, (uint8_t*)hw_model, HARDWARE_MODEL_SIZE));
    // Model and variant values of 0xFFFF are acceptable
    *model = ((uint32_t)hw_model[1] << 8) | (uint32_t)hw_model[0];
    *variant = ((uint32_t)hw_model[3] << 8) | (uint32_t)hw_model[2];
    return SYSTEM_ERROR_NONE;
}

#ifndef HAL_DEVICE_ID_NO_DCT
int hal_get_device_secret(char* data, size_t size, void* reserved)
{
    // Check if the device secret data is initialized in the DCT
    char secret[HAL_DEVICE_SECRET_SIZE] = {};
    static_assert(sizeof(secret) == DCT_DEVICE_SECRET_SIZE, "");
    int ret = dct_read_app_data_copy(DCT_DEVICE_SECRET_OFFSET, secret, sizeof(secret));
    if (ret < 0) {
        return ret;
    }
    if (!isPrintable(secret, sizeof(secret))) {
        // Check the OTP memory
        CHECK(readLogicalEfuse(MOBILE_SECRET_OFFSET, (uint8_t*)secret, sizeof(secret)));
        if (!isPrintable(secret, sizeof(secret))) {
            return SYSTEM_ERROR_NOT_FOUND;
        };
    }
    if (data && size > 0) {
        memcpy(data, secret, std::min(size, sizeof(secret)));
        if (size > HAL_DEVICE_SECRET_SIZE) {
            data[HAL_DEVICE_SECRET_SIZE] = 0;
        }
    }
    return HAL_DEVICE_SECRET_SIZE;
}
#endif /* HAL_DEVICE_ID_NO_DCT */

