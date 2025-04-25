/*

#include "ble_transmitter_i.h"
#include <furi.h>
#include <lib/flipper/ble/ble.h>
#include <string.h>

#define DEFAULT_BLE_TX_POWER 4
#define DEFAULT_BLE_NAME     "FlipperBLE"

static void ble_transmitter_init_cfg_model(void* context) {
    furi_assert(context);
    BleTransmitter* transmitter = context;
    transmitter->model->tx_power = DEFAULT_BLE_TX_POWER;
    strncpy(
        transmitter->model->device_name,
        DEFAULT_BLE_NAME,
        sizeof(transmitter->model->device_name));
}

static void ble_transmitter_init(void* context) {
    ble_transmitter_init_cfg_model(context);
    ble_init();
    ble_advertise_start();
}

BleTransmitter* ble_transmitter_alloc(void* context, BleTransmitterSendMethod send_method) {
    furi_assert(context);
    BleTransmitter* transmitter = malloc(sizeof(BleTransmitter));
    transmitter->model = malloc(sizeof(BleTransmitterModel));
    transmitter->context = context;
    transmitter->send_method = send_method;
    ble_transmitter_init(transmitter);
    return transmitter;
}

void ble_transmitter_free(BleTransmitter* transmitter) {
    if(transmitter) {
        free(transmitter->model);
        free(transmitter);
    }
}

void ble_transmitter_send_msg(BleTransmitter* transmitter, const char* msg) {
    furi_assert(transmitter);
    transmitter->send_method(transmitter->context, msg, strlen(msg));
}
*/
