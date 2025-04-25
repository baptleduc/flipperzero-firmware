#include <furi.h>
#include <furi_hal_bt.h>
#include <services/serial_service.h>

void ble_transmitter_init() {
    FuriHalBleProfileParams params = {0};
    FuriHalBleProfileBase* base = ble_profile_serial_start(params);
    furi_hal_bt_init();
    furi_hal_bt_start_advertising();
}

void ble_transimitter_send(uint8_t* data) {
    BleHidInstance* ble_hid = malloc(sizeof(BleHidInstance));
    ble_hid->bt = furi_record_open(RECORD_BT);
}

int32_t ble_sender_app_main(void* p) {
    UNUSED(p);

    const char* msg = "Hello World";

    while(1) {
        furi_delay_ms(1000);
        ble_profile_serial_tx(profile, (const uint8_t*)msg, strlen(msg));
    }
    return 0;
}
