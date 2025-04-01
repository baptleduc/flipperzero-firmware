#ifndef UART_DEMO_H
#define UART_DEMO_H

#include <expansion/expansion.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/view_dispatcher.h>
#include <gui/canvas.h>
#include <input/input.h>

#include "uart_helper.h"


#define DEVICE_BAUDRATE 9600
#define DEFAULT_DR 5
#define DEFAULT_TX_POWER 14
#define APPKEY "2FFA5393E446D6CEC8EB9D9AFA5521F3"
#define PORT 10


// Comment out the following line to process data as it is received.
#define DEMO_PROCESS_LINE
#define LINE_DELIMITER         '\n'
#define INCLUDE_LINE_DELIMITER false

typedef struct {
    uint8_t  margin;          // Link margin in dB (0-254) from the last LinkCheckReq.
    uint16_t gateway_count;   // Number of gateways that received the last transmitted frame.
    uint8_t  rx_window;       // RX window number where the last frame was received.
    int8_t   rssi;            // RSSI (Received Signal Strength Indicator) of the last frame.
    int8_t   snr;             // SNR (Signal-to-Noise Ratio) of the last frame.
    uint8_t  port;            // Port number used in the last transmission.
    char *   data;            // Payload data of the last transmitted frame.
    bool     is_multicast;    // True if the frame was received in a multicast group.
    bool     is_pending;      // True if the server has pending data for the device.
    bool     is_ack;         // True if the last frame was acknowledged by the server.
} LoRaMsgResponse;

typedef struct {
    Gui *gui;
    FuriTimer *timer;
    ViewDispatcher *view_dispatcher;
    Submenu *submenu;
    uint32_t index;
    UartHelper *uart_helper;
    FuriString *send_cmd;
    LoRaMsgResponse *msg_response;
    uint32_t lora_bitmask; // Bitmask to store the state of the LoRa device
} UartDemoApp;

typedef enum{
    CONFIG = 1, // Being on this state means the device has been configured (e.g. AppKey, DR, TxPower)
    JOINED = 2, // Being on this state means the device has joined the network
    SENDING = 4, // Being on this state means the device is sending data
} LoraStateFlags;

typedef enum {
    UartDemoSubMenuViewId = 1,
} UartDemoViewIds;



#define DEBUG_LORA_MSG_RESPONSE(msg_response) \
    FURI_LOG_I("DebugMsgResponse", "LoRaMsgResponse Debug:"); \
    FURI_LOG_I("DebugMsgResponse", "  Port: %hhu", (msg_response).port); \
    FURI_LOG_I("DebugMsgResponse", "  Data: %s", (msg_response).data); \
    FURI_LOG_I("DebugMsgResponse", "  Margin: %hhu", (msg_response).margin); \
    FURI_LOG_I("DebugMsgResponse", "  Gateway Count: %u", (msg_response).gateway_count); \
    FURI_LOG_I("DebugMsgResponse", "  RX Window: %d", (msg_response).rx_window); \
    FURI_LOG_I("DebugMsgResponse", "  RSSI: %d dBm", (msg_response).rssi); \
    FURI_LOG_I("DebugMsgResponse", "  SNR: %d dB", (msg_response).snr); \
    FURI_LOG_I("DebugMsgResponse", "  Multicast: %s", (msg_response).is_multicast ? "Yes" : "No"); \
    FURI_LOG_I("DebugMsgResponse", "  Pending: %s", (msg_response).is_pending ? "Yes" : "No"); \
    FURI_LOG_I("DebugMsgResponse", "  ACK Received: %s", (msg_response).is_ack ? "Yes" : "No");

// Callback to handle UART responses
void handle_default_response(FuriString * line, void *context);
void handle_msg_response(FuriString * line, void *context);
void handle_join_response(FuriString * line, void *context);



#endif
