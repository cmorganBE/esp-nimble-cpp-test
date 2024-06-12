/**
 * With bonding and whitelists the iOS device is not able to consistently reconnect
 * to the device. Test process is as follows:
 *
 * - Erase bond on iOS side
 * - idf.py build erase-flash flash monitor (erase-flash is to ensure no persisted NVS remains)
 * - Attempt to pair during whitelist period (fails)
 * - Attempt to pair during any period (success)
 * - Disconnect
 * - Attempt to reconnect during whitelist period (fails)
 *
 *
 * Test showing how to use whitelists and connection configuration to
 * permit connection of bonded connections only in one mode and any connection
 * (plus bonding) in another.
 *
 * Use case is a device where only when it is put into a special mode, a button is pressed etc
 * can new devices be connected and bonded. Otherwise only previously bonded devices are allowed
 * to connect. This is a temporal security feature, not great but for devices without any
 * display it is better than nothing.
 *
 *
 *
 * #ifdef WISH_THIS_WORKED attempts to use secure connections. Unfortunately with SC enabled
 *   the iOS device is unable to reconnect after bonding, when in 'whitelist' mode.
 *
 * #ifndef WISH_THIS_WORKED disables SC
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

#undef WISH_THIS_WORKED

extern "C"
{
    void app_main(void);
}

enum ConnectMode {
    Any,
    WhitelistOnly
};

ConnectMode connectMode;

ConnectMode getConnectMode()
{
    return connectMode;
}

void updateConnectMode(NimBLEAdvertising *pAdvertising, ConnectMode newConnectMode)
{
    connectMode = newConnectMode;
    if(connectMode == ConnectMode::WhitelistOnly)
    {
        ESP_LOGW(TAG, "*************** connectMode -> whitelist only");
        pAdvertising->setScanFilter(false, true); // connecting with whitelisted connections ONLY
    } else
    {
        ESP_LOGW(TAG, "*************** connectMode -> any");
        pAdvertising->setScanFilter(false, false); // scan and connecting without whitelist checks
    }
}

class ServerCallbacks: public NimBLEServerCallbacks
{
public:
    ServerCallbacks() {}

    static void PrintConnctionInfo(NimBLEConnInfo &connInfo) {
        ESP_LOGI(TAG, "OTA address %s, type %d", connInfo.getAddress().toString().c_str(), connInfo.getAddress().getType());
        ESP_LOGI(TAG, "ID address %s, type %d", connInfo.getIdAddress().toString().c_str(), connInfo.getIdAddress().getType());
        ESP_LOGI(TAG, "Bonded: %s, Authenticated: %s, Encrypted: %s, Key size: %d",
            connInfo.isBonded() ? "yes" : "no",
            connInfo.isAuthenticated() ? "yes" : "no",
            connInfo.isEncrypted() ? "yes" : "no",
            connInfo.getSecKeySize());
    }

    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override
    {
        ESP_LOGI(TAG, "onConnect");

        // updateConnParams() to shorten te connection interval to
        // improve performance
        auto minInterval = 6; // 6 * 1.25 = 7.5ms
        auto maxInterval = minInterval;
        auto latency_packets = 0;
        auto timeout = 10; // 10 * 10ms = 100ms
        pServer->updateConnParams(connInfo.getConnHandle(),
                                  minInterval,
                                  maxInterval,
                                  latency_packets,
                                  timeout);
        ESP_LOGI(TAG, "mtu: %d", connInfo.getMTU());
        PrintConnctionInfo(connInfo);
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        ESP_LOGI(TAG, "Client disconnected");
    };

    void onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) override {
        ESP_LOGI(TAG, "MTU updated: %u for connection ID: %u", MTU, connInfo.getConnHandle());
    };

    void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
        ESP_LOGI(TAG, "onAuthenticationComplete()");
        /** Check that encryption was successful, if not we disconnect the client */
        if(!connInfo.isEncrypted()) {
            ESP_LOGW(TAG, "Encrypt connection failed - disconnecting client");
            NimBLEDevice::getServer()->disconnect(connInfo.getConnHandle());
            return;
        } else {
            ESP_LOGI(TAG, "connection is encrypted");
        }

// Only add the ID address to the whitelist, this is the one that remain consistent
// when connecting to the device(?)
        NimBLEDevice::whiteListAdd(connInfo.getIdAddress());

        PrintConnctionInfo(connInfo);
    };

    // Don't expect to get this call but want an informational printout if we do
    uint32_t onPassKeyRequest() override
    {
        ESP_LOGE(TAG, "onPassKeyRequest()");
        return 0;
    }

    // Don't expect to get this call but want an informational printout if we do
    bool onConfirmPIN(uint32_t pin) override
    {
        ESP_LOGE(TAG, "onConfirmPin()");
        return false;
    }
};

void app_main(void)
{
    ESP_LOGI(TAG, "Hello world!");

    NimBLEDevice::init("NimBLE Test");

    ESP_LOGI(TAG, "Bonded devices:");
    for (int i = 0; i < NimBLEDevice::getNumBonds(); i++)
    {
        ESP_LOGI(TAG, "%d.: %s", i, NimBLEDevice::getBondedAddress(i).toString().c_str());
    }

#ifdef WISH_THIS_WORKED
    // Will not reconnect after pairing and bonding when setScanFilter(false, true) (whitelist mode)
    NimBLEDevice::setSecurityAuth(true, false, true);
#else
    // This doesn't seem to work consistently either, appears to have nothing to do with SC setting...
    NimBLEDevice::setSecurityAuth(true, false, false);
#endif

    NimBLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
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

    updateConnectMode(pAdvertising, ConnectMode::WhitelistOnly);

    NimBLEDevice::startAdvertising();
    ESP_LOGI(TAG, "Characteristic defined! Now you can read it in your phone!\n");

    ConnectMode newConnectMode = getConnectMode();
    const auto connect_toggle_ms = 30000;
    auto toggle_ms = 0;
    const auto loop_sleep_time_ms = 1000;

#define nextMode() ((getConnectMode() == ConnectMode::Any) ? ConnectMode::WhitelistOnly : ConnectMode::Any)

    while(true)
    {
        vTaskDelay(pdMS_TO_TICKS(loop_sleep_time_ms));

        // changing the connection mode appears to require stopping advertizing during
        // the change
        if(newConnectMode != getConnectMode())
        {
            pAdvertising->stop();
            updateConnectMode(pAdvertising, newConnectMode);
            pAdvertising->start();
        }

        toggle_ms += loop_sleep_time_ms;

        ESP_LOGI(TAG, "switching to %s in %d ms", nextMode() == ConnectMode::Any ? "Any" : "WhitelistOnly", (connect_toggle_ms - toggle_ms));

        // toggle the connect mode every 'connect_toggle_ms' time
        if(toggle_ms >= connect_toggle_ms)
        {
            newConnectMode = (getConnectMode() == ConnectMode::Any) ? ConnectMode::WhitelistOnly : ConnectMode::Any;
            toggle_ms = 0;
        }
    }
}
