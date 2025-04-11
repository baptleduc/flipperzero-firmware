#ifndef UART_DEMO_H
#define UART_DEMO_H

#include <expansion/expansion.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/view_dispatcher.h>
#include <gui/canvas.h>
#include <input/input.h>

#include "uart_helper.h"
#include "views/lora_receiver.h"
#include "lora_custom_event.h"

#define DEVICE_BAUDRATE  9600
#define DEFAULT_DR       5
#define DEFAULT_TX_POWER 14
#define APPKEY           "2FFA5393E446D6CEC8EB9D9AFA5521F3"
#define PORT             10

// Comment out the following line to process data as it is received.
#define DEMO_PROCESS_LINE
#define LINE_DELIMITER         '\n'
#define INCLUDE_LINE_DELIMITER false

typedef enum {
    INIT,                       // Being on this state means the device is not configured yet
    CONFIG,                     // Being on this state means the device has been configured (e.g. AppKey, DR, TxPower)
    JOINED,                     // Being on this state means the device has joined the network
    SENDING,                    // Being on this state means the device is sending data
    RX,                         // Being on this state means the device is receiving data
} LoraStateFlag;

typedef enum {
    DEFAULT_CMD = 0,
    MSG_CMD = 1,
    CMSG_CMD = 2,
    JOIN_CMD = 3,
    CONFIG_CMD = 4,
    CMD_TYPE_COUNT,
} LoraCmdType;

typedef struct {
    Gui *gui;
    FuriTimer *timer;
    ViewDispatcher *view_dispatcher;
    SceneManager *scene_manager;
    Submenu *submenu;
    TextBox *text_box;
    uint32_t index;
    UartHelper *uart_helper;
    FuriString *send_cmd;
    LoraReceiver *receiver;
    LoraStateFlag current_state;
} LoraApp;

typedef enum {
    LoraAppSubMenuView,
    LoraAppTextBoxView,
} LoraAppView;

#define DEBUG_LORA_MSG_RESPONSE(msg_response)                                                  \
    FURI_LOG_D("DebugMsgResponse", "LoRaMsgResponseModel Debug:");                             \
    FURI_LOG_D("DebugMsgResponse", "  Port: %hhu", (msg_response).port);                       \
    FURI_LOG_D("DebugMsgResponse", "  Data: %s", (msg_response).data);                         \
    FURI_LOG_D("DebugMsgResponse", "  Margin: %hhu", (msg_response).margin);                   \
    FURI_LOG_D("DebugMsgResponse", "  Gateway Count: %u", (msg_response).gateway_count);       \
    FURI_LOG_D("DebugMsgResponse", "  RX Window: %d", (msg_response).rx_window);               \
    FURI_LOG_D("DebugMsgResponse", "  RSSI: %d dBm", (msg_response).rssi);                     \
    FURI_LOG_D("DebugMsgResponse", "  SNR: %d dB", (msg_response).snr);                        \
    FURI_LOG_D(                                                                                \
        "DebugMsgResponse", "  Multicast: %s", (msg_response).is_multicast ? "Yes" : "No");    \
    FURI_LOG_D("DebugMsgResponse", "  Pending: %s", (msg_response).is_pending ? "Yes" : "No"); \
    FURI_LOG_D("DebugMsgResponse", "  ACK Received: %s", (msg_response).is_ack ? "Yes" : "No");

// Callback to handle UART responses
void handle_default_response(FuriString * line, void *context);
void handle_msg_response(FuriString * line, void *context);
void handle_join_response(FuriString * line, void *context);

/**
 * @brief Handles the reception of a LoRa packet in TEST mode (P2P LoRa).
 *
 * This function is triggered on the receiver side when a LoRa packet is 
 * received from the transmitter during TEST mode operation. It decodes the
 * data and updates the LoRaMsgResponseModel structure with the received data.
 */
void handle_rx_response(FuriString * line, void *context);

void lora_enter_receive_mode(void *context);
void send_cmsg(LoraApp * app, const char *msg);
void setup_lora_connexion(void *context);
void otaa_join_procedure(void *context);
#endif
