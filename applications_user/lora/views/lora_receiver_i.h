#pragma once

#include "lora_receiver.h"
#include "../lora_custom_event.h"
#include <furi.h>

#define MAX_DATA_SIZE (1 << 8)  // 256 bytes
#define MAX_CANAL_NUM (8)       // (1-8)

// Default values for LoRa configuration
#define DEFAULT_FREQ                (868) // Frequency in MHz
#define DEFAULT_CANAL_NUM           (1) // Canal 1
#define DEFAULT_SF                  (12) // Spreading Factor 12
#define DEFAULT_BW                  (125) // Bandwidth 125 kHz
#define DEFAULT_TX_PREAMBLE         (12) // TX Preamble length
#define DEFAULT_RX_PREAMBLE         (15) // RX Preamble length
#define DEFAULT_POWER               (14) // Power level (dBm)
#define DEFAULT_WITH_CRC            (true) // CRC enabled
#define DEFAULT_IQ_INVERTED         (false) // IQ inversion disabled
#define DEFAULT_WITH_PUBLIC_LORAWAN (false) // Public LoRaWAN enabled

typedef struct {
    uint8_t margin;             // Link margin in dB (0-254) from the last LinkCheckReq.
    uint16_t gateway_count;     // Number of gateways that received the last transmitted frame.
    uint8_t rx_window;          // RX window number where the last frame was received.
    int8_t rssi;                // RSSI (Received Signal Strength Indicator) of the last frame.
    int8_t snr;                 // SNR (Signal-to-Noise Ratio) of the last frame.
    uint8_t port;               // Port number used in the last transmission.
    char data[MAX_DATA_SIZE];   // Payload data of the last transmitted frame.
    char decoded_data[MAX_DATA_SIZE]; // Decoded data of the last transmitted frame.
    bool is_multicast;          // True if the frame was received in a multicast group.
    bool is_pending;            // True if the server has pending data for the device.
    bool is_ack;                // True if the last frame was acknowledged by the server.
} LoraMsgResponseModel;

/**
 * @brief LoRa configuration Object
 */
typedef struct {
    uint32_t freq;              // Frequency in MHz
    uint8_t canal;              // Canal number (1-8)
    uint8_t sf;                 // Spreading factor
    uint8_t bw;                 // Bandwidth
    uint8_t tx_preamble;        // TX preamble length
    uint8_t rx_preamble;        // RX preamble length
    uint8_t power;              // Power level (dbm)
    bool with_crc;              // CRC enabled/disabled
    bool is_iq_inverted;        // IQ inversion enabled/disabled
    bool with_public_lorawan;   // Public LoRaWAN enabled/disabled
} LoraConfigModel;

/**
 * @brief LoRa Receiver Model
 */
typedef struct {
    LoraMsgResponseModel msg_response; // Model for the LoRa message response
    LoraConfigModel config;     // Model for the LoRa configuration
} LoraReceiverModel;

/**
 * @brief LoRa receiver Object
 */
struct LoraReceiver {
    View *view;
    void *context;
    LoraReceiverProcessCallback callback; // Callback function for received messages
    LoraStateManager *state_manager;
};

// DEBUG MACRO
#define DEBUG_LORA_MSG_RESPONSE(lora_receiver)                                                   \
    FURI_LOG_D("DebugMsgResponse", "LoRaMsgResponseModel Debug:");                               \
    FURI_LOG_D("DebugMsgResponse", "  Port: %hhu", (lora_receiver->model->msg_response).port);   \
    FURI_LOG_D("DebugMsgResponse", "  Data: %s", (lora_receiver->model->msg_response).data);     \
    FURI_LOG_D(                                                                                  \
        "DebugMsgResponse", "  Margin: %hhu", (lora_receiver->model->msg_response).margin);      \
    FURI_LOG_D(                                                                                  \
        "DebugMsgResponse",                                                                      \
        "  Gateway Count: %u",                                                                   \
        (lora_receiver->model->msg_response).gateway_count);                                     \
    FURI_LOG_D(                                                                                  \
        "DebugMsgResponse", "  RX Window: %d", (lora_receiver->model->msg_response).rx_window);  \
    FURI_LOG_D("DebugMsgResponse", "  RSSI: %d dBm", (lora_receiver->model->msg_response).rssi); \
    FURI_LOG_D("DebugMsgResponse", "  SNR: %d dB", (lora_receiver->model->msg_response).snr);    \
    FURI_LOG_D(                                                                                  \
        "DebugMsgResponse",                                                                      \
        "  Multicast: %s",                                                                       \
        (lora_receiver->model->msg_response).is_multicast ? "Yes" : "No");                       \
    FURI_LOG_D(                                                                                  \
        "DebugMsgResponse",                                                                      \
        "  Pending: %s",                                                                         \
        (lora_receiver->model->msg_response).is_pending ? "Yes" : "No");                         \
    FURI_LOG_D(                                                                                  \
        "DebugMsgResponse",                                                                      \
        "  ACK Received: %s",                                                                    \
        (lora_receiver->model->msg_response).is_ack ? "Yes" : "No");
