#pragma once

#include <stdbool.h>

#define NFC_SETTINGS_FILE_NAME ".nfc.settings"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool parser_debug;
} NfcSettings;

bool nfc_settings_save(NfcSettings* settings);

void nfc_settings_load(NfcSettings* settings);

#ifdef __cplusplus
}
#endif
