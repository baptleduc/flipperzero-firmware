#pragma once

#include <furi.h>

extern const uint32_t bandwidth_list[];
extern const uint8_t canal_list[];
/**
 * @brief LoRa configuration Object
 */
typedef struct {
    uint32_t freq;              // Frequency in MHz
    uint8_t canal_idx;          // Canal index (see canal_list)
    uint8_t sf;                 // Spreading factor
    uint8_t bw_idx;             // Bandwidth index (see bandwidth_list)
    uint8_t tx_preamble;        // TX preamble length
    uint8_t rx_preamble;        // RX preamble length
    uint8_t power;              // Power level (dbm)
    bool with_crc;              // CRC enabled/disabled
    bool is_iq_inverted;        // IQ inversion enabled/disabled
    bool with_public_lorawan;   // Public LoRaWAN enabled/disabled
} LoraConfigModel;
