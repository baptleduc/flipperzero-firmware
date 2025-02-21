#include "nfc_settings.h"

#include <furi.h>
#include <storage/storage.h>
#include <toolbox/saved_struct.h>

#define TAG "NfcSettings"

#define NFC_SETTINGS_PATH         INT_PATH(NFC_SETTINGS_FILE_NAME)
#define NFC_SETTINGS_FILE_VERSION (0)
#define NFC_SETTINGS_FILE_MAGIC   (0x99)

bool nfc_settings_save(NfcSettings* settings) {
    furi_assert(settings);

    return saved_struct_save(
        NFC_SETTINGS_PATH,
        settings,
        sizeof(NfcSettings),
        NFC_SETTINGS_FILE_MAGIC,
        NFC_SETTINGS_FILE_VERSION);
}

void nfc_settings_load(NfcSettings* settings) {
    furi_assert(settings);

    const bool success = saved_struct_load(
        NFC_SETTINGS_PATH,
        settings,
        sizeof(NfcSettings),
        NFC_SETTINGS_FILE_MAGIC,
        NFC_SETTINGS_FILE_VERSION);

    if(!success) {
        FURI_LOG_W(TAG, "Failed to load settings, using defaults");
        memset(settings, 0, sizeof(NfcSettings));
        nfc_settings_save(settings);
    }
}
