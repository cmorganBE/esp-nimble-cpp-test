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

    // Does not appear to affect behavior of this test
//    NimBLEDevice::setMTU(251);
//    ESP_LOGI(TAG, "device mtu %d", NimBLEDevice::getMTU());

    ESP_LOGI(TAG, "Bonded devices:");
    for (int i = 0; i < NimBLEDevice::getNumBonds(); i++)
    {
        ESP_LOGI(TAG, "%d.: %s", i, NimBLEDevice::getBondedAddress(i).toString().c_str());
    }



    // NOTE: bleprph example code results in iOS pairing dialog as soon as
    // connecting to the device. This example, either Setting A or B, only
    // requests pairing when the characteristic is accessed.



    // Setting A - WORKS
    // Pairs and bonds and will reconnect, even after esp32 reboot
//    NimBLEDevice::setSecurityAuth(true, false, false);

    // Setting B - DOESNT WORK
    // Will pair and bond but will NOT reconnect
    NimBLEDevice::setSecurityAuth(true, false, true);



    // NimBLEDevice defaults to 'no input output'
//    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    NimBLEServer *pServer = BLEDevice::createServer();
    NimBLEService *pService = pServer->createService(SERVICE_UUID);
    NimBLECharacteristic *pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
            NIMBLE_PROPERTY::READ |
            NIMBLE_PROPERTY::READ_ENC |
            NIMBLE_PROPERTY::WRITE |
            NIMBLE_PROPERTY::WRITE_ENC
        );

    pCharacteristic->setValue("Hello World says Neil");
    pService->start();

    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);

    // Does not appear to affect bonding or pairing
//    pAdvertising->setScanResponse(true);
//    pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
//    pAdvertising->setMinPreferred(0x12);

    NimBLEDevice::startAdvertising();
    ESP_LOGI(TAG, "Characteristic defined! Now you can read it in your phone!\n");
}