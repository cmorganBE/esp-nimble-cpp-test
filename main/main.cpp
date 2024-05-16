/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_log.h"

#include <NimBLEDevice.h>

#define SERVICE_UUID "acdab998-f4ce-4f5d-a049-15d597803500"
#define CHARACTERISTIC_UUID "acdab998-f4ce-4f5d-a049-15d597803501"

static const char *TAG = "main";

extern "C"
{
    void app_main(void);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Hello world!");

    NimBLEDevice::init("NimBLE Test");

    ESP_LOGI(TAG, "Bonded devices:");
    for (int i = 0; i < NimBLEDevice::getNumBonds(); i++)
    {
        ESP_LOGI(TAG, "%d.: %s", i, NimBLEDevice::getBondedAddress(i).toString().c_str());
    }

    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
    NimBLEDevice::setSecurityPasskey(123456);
    NimBLEServer *pServer = BLEDevice::createServer();
    NimBLEService *pService = pServer->createService(SERVICE_UUID);
    NimBLECharacteristic *pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::READ |
            NIMBLE_PROPERTY::READ_AUTHEN |
            NIMBLE_PROPERTY::WRITE |
            NIMBLE_PROPERTY::WRITE_AUTHEN);

    pCharacteristic->setValue("Hello World says Neil");
    pService->start();

    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);

    NimBLEDevice::startAdvertising();
    ESP_LOGI(TAG, "Characteristic defined! Now you can read it in your phone!\n");
}