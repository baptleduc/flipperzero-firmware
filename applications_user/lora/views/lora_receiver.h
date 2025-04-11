#pragma once

#include <gui/view.h>

#define MAX_DATA_SIZE (1 << 8)  // 256 bytes

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
} LoRaMsgResponseModel;

/**
 * @brief LoRa receiver Object
 */
typedef struct {
    View *view;
    void *context;
    LoRaMsgResponseModel *msg_response;
} LoraReceiver;

LoraReceiver *lora_receiver_alloc(void);
