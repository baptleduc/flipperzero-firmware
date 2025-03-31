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
#define APPKEY "1234567890ABCDEF1234567890ABCDEF"
#define PORT 10


// Comment out the following line to process data as it is received.
#define DEMO_PROCESS_LINE
#define LINE_DELIMITER         '\n'
#define INCLUDE_LINE_DELIMITER false


typedef struct {
    Gui *gui;
    FuriTimer *timer;
    ViewDispatcher *view_dispatcher;
    Submenu *submenu;
    uint32_t index;
    UartHelper *uart_helper;
    FuriString *send_cmd;
} UartDemoApp;

typedef enum {
    UartDemoSubMenuViewId = 1,
} UartDemoViewIds;

void handle_default_response(FuriString * line, void *context);
void handle_msg_response(FuriString * line, void *context);
void handle_cmsg_response(FuriString * line, void *context);



#endif
