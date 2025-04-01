#include <stdlib.h>
#include "uart_demo.h"


// Global variables
int dr = DEFAULT_DR;            // Data rate
int tx_power = DEFAULT_TX_POWER; // Transmit power

/**
 * This callback function is called when a submenu item is clicked.
 * 
 * @param context The context passed to the submenu.
 * @param index   The index of the submenu item that was clicked.
*/
static void uart_demo_submenu_item_callback(void *context, uint32_t index);

/**
 * Adds the default submenu entries.
 * 
 * @param submenu The submenu.
 * @param context The context to pass to the submenu item callback function.
*/
static void uart_demo_submenu_add_default_entries(Submenu *submenu,
                                                  void *context)
{
    UartDemoApp *app = context;
    submenu_reset(submenu);
    submenu_add_item(submenu, "Clear", 0, uart_demo_submenu_item_callback,
                     context);
    submenu_add_item(submenu, "OTAA JOIN", 1,
                     uart_demo_submenu_item_callback, context);
    submenu_add_item(submenu, "Send Msg 2", 2,
                     uart_demo_submenu_item_callback, context);

    app->index = 3;
}

static void otaa_join_procedure(void *context)
{
    UartDemoApp *app = context;
    int join_attempts = 1;
    
    // Loop until the device is joined to the network
    while (!(app->lora_bitmask & JOINED)) {
        FURI_LOG_I("OTAA_JOIN", "Attempting to join the network...");
        // Send the join command
        uart_helper_send(app->uart_helper, "AT+JOIN\n", 9, JOIN);
        furi_delay_ms(10000);
        
        if(app->lora_bitmask & JOINED) {
            // Device is joined, break the loop
            break;
        }

        join_attempts++;

        if(join_attempts > 3){
            // If the device is not joined after 3 attempts, break the loop
            FURI_LOG_I("OTAA_JOIN", "Failed to join after 3 attempts");
            break;
        }

        FURI_LOG_I("OTAA_JOIN", "Join attempt %d", join_attempts);
        
    }

}

static void setup_lora_connexion(void *context)
{
    UartDemoApp *app = context;

    uart_helper_send(app->uart_helper, "AT+ID\n", 7, DEFAULT_MSG_TYPE);
    furi_delay_ms(1000);

    uart_helper_send(app->uart_helper, "AT+MODE=LWOTAA\n", 16,
                     DEFAULT_MSG_TYPE);
    furi_delay_ms(1000);

    furi_string_printf(app->send_cmd, "AT+DR=%d\n", dr);
    uart_helper_send_string(app->uart_helper, app->send_cmd,
                            DEFAULT_MSG_TYPE);
    furi_delay_ms(1000);

    furi_string_printf(app->send_cmd, "AT+POWER=%d\n", tx_power);
    uart_helper_send_string(app->uart_helper, app->send_cmd,
                            DEFAULT_MSG_TYPE);
    furi_delay_ms(1000);

    uart_helper_send(app->uart_helper, "AT+ADR=ON\n", 11,
                     DEFAULT_MSG_TYPE);
    furi_delay_ms(1000);

    uart_helper_send(app->uart_helper, "AT+CLASS=A\n", 12,
                     DEFAULT_MSG_TYPE);
    furi_delay_ms(1000);

    furi_string_printf(app->send_cmd, "AT+KEY=APPKEY,%s\n", APPKEY);
    uart_helper_send_string(app->uart_helper, app->send_cmd,
                            DEFAULT_MSG_TYPE);
    furi_delay_ms(1000);

    // TODO : change because we need to pass to a specific callback
    app->lora_bitmask |= CONFIG;

}
static void send_cmsg(UartDemoApp *app, const char *msg)
{   
    furi_string_printf(app->send_cmd, "AT+CMSG=%s\n", msg);
    uart_helper_send_string(app->uart_helper, app->send_cmd,
                            CMSG);
    furi_delay_ms(1000);
}
static void uart_demo_submenu_item_callback(void *context, uint32_t index)
{
    UartDemoApp *app = context;

    if (index == 0) {
        // Clear the submenu and add the default entries.
        uart_demo_submenu_add_default_entries(app->submenu, app);
    } else if (index == 1) {
        setup_lora_connexion(app);
        otaa_join_procedure(app);
    } else if (index == 2) {
        // Send a confirmed message to the server.
        send_cmsg(app, "Hello World");
    } else {
        // The item was received data.
    }
}

// static uint64_t convert_char_to_uint64(const char* cstr) {
//     if (cstr == NULL || *cstr < '0' || *cstr > '9') {
//         return 0;  
//     }
//     return (uint64_t)(*cstr - '0');
// }


static int parse_msg_response(FuriString* line, LoRaMsgResponse* msg_response) {
    if (!line || !msg_response) return -1;

    // Parse the line to extract the fields
    size_t index;
    FURI_LOG_D("parse_msg_response", "%s",
               furi_string_get_cstr(line));
    // Get delimiter `:` index
    index = furi_string_search_char(line, ':', 0);
    if (index == FURI_STRING_FAILURE) {
        return -1; // No delimiter found
    }
    // Extract the right part of the line (e.g +CMSG: Port:0, we extract the right part Port:0 )
    furi_string_right(line, index + 1); // Trim

    index = furi_string_search_str(line, "FPENDING", 0);
    if (index != FURI_STRING_FAILURE) {
        msg_response->is_pending = true;
        return 0;
    }

    index = furi_string_search_str(line, "Link ", 0);
    if (index != FURI_STRING_FAILURE) {
        char buffer_margin[4] = {0}; // Buffer to store extracted numbers as strings
        size_t i = 0, j= 0;
        char c;
        // Read the first number (margin)
        while (i < 5) {
            c = furi_string_get_char(line, index + 5 + i); // "Link " is 5 characters long
            if (c == ',' || c == '\0') break; // Stop at ',' or end of string
            buffer_margin[i++] = c;
        }
        msg_response->margin = (uint8_t)atoi(buffer_margin);

        char buffer_gw[4] = {0};

        // Read the second number (gateway_count)
        if (c == ',') { // Ensure there is a second value
            i++; // Skip the comma
            while (i < 5) {
                char c2 = furi_string_get_char(line, index + 5 + i);
                if (c2 == '\0') break; // Stop at end of string
                buffer_gw[j++] = c2;
            }
            msg_response->gateway_count = (uint8_t)atoi(buffer_gw); // Convert to integer
        }
        return 0;
    }

    index = furi_string_search_str(line, "RXWIN", 0);
    if (index != FURI_STRING_FAILURE) {
        msg_response->rx_window = (uint8_t)(furi_string_get_char(line, index + 5) - '0'); // Convert char to int

        char buffer_rssi[4] = {0}; // Buffer to store RSSI value
        size_t i = 0;
        // Read the RSSI value
        while (i < 3) {
            char c = furi_string_get_char(line, index + 14 + i); // "RXWIN1, RSSI " is 13 characters long, we use +1 to skip the minus
            if (c == ',' || c == '\0') break; // Stop at ',' or end of string
            buffer_rssi[i++] = c;
        }
        msg_response->rssi = -(int8_t)atoi(buffer_rssi); // Convert to integer

        char buffer_snr[4] = {0}; // Buffer to store SNR value
        bool is_negative = false;
        size_t j = 0;
        // Read the SNR value
        while (j < 3) {
            char c = furi_string_get_char(line, index + 14 + i + j + 6); // "SNR " is 3 characters long + 3 to skip the comma and space before and after
            if (c == ',' || c == '\0') break; // Stop at ',' or end of string
            if (c == '-') {
                is_negative = true; // Mark as negative
                continue;
            }
            buffer_snr[j++] = c;
        }
        if (is_negative) {
            msg_response->snr = -(int8_t)atoi(buffer_snr); // Convert to integer
        } else {
            msg_response->snr = (int8_t)atoi(buffer_snr); // Convert to integer
        }
        FURI_LOG_D("parse_msg_response", "snr buffer: %s",
                   buffer_snr);
    }

    index = furi_string_search_str(line, "ACK Received", 0);
    if (index != FURI_STRING_FAILURE) {
        msg_response->is_ack = true;
        return 0;
    }

    index = furi_string_search_str(line, "MULTICAST", 0);
    if (index != FURI_STRING_FAILURE) {
        msg_response->is_multicast = true;
        return 0;
    }

    index = furi_string_search_str(line, "PORT: ", 0);
    if (index != FURI_STRING_FAILURE) {
        char buffer[4] = {0}; // Permettre jusqu'à 3 chiffres + terminaison
        size_t i = 0;

        // Read Port characters until ';' or null terminator
        while (i < 3) {
            char c = furi_string_get_char(line, index + 6 + i); // "PORT: " = 6 char
            if (c == ';' || c == '\0') break; // End if we reach ';' or null terminator
            buffer[i++] = c;
        }
        
        msg_response->port = (uint8_t)atoi(buffer);
    }

    index = furi_string_search_str(line, "RX: ", 0);
    if (index != FURI_STRING_FAILURE) {
        const char* data_start = furi_string_get_cstr(line) + index + 1; // Skip "RX: "
        strcpy(msg_response->data, data_start);
        return 0;
    }

    return 0;
}

void handle_msg_response(FuriString *line, void *context)
{
    UartDemoApp *app = context;
    FURI_LOG_I("handle_cmsg_response", "%s",
               furi_string_get_cstr(line));
    parse_msg_response(line, app->msg_response);
    DEBUG_LORA_MSG_RESPONSE(*app->msg_response);
}
void handle_join_response(FuriString *line, void *context)
{
    FURI_LOG_I("handle_join_response", "%s",
               furi_string_get_cstr(line));
    UartDemoApp *app = context;
    if (furi_string_start_with(line, "+JOIN: Network joined")) {
        app->lora_bitmask |= JOINED;
        FURI_LOG_I("handle_join_response", "Network joined");
        return;
    } 
}
#ifdef DEMO_PROCESS_LINE
void handle_default_response(FuriString *line, void *context)
{
    (void) context;
    // submenu_add_item(
    //     app->submenu,
    //     furi_string_get_cstr(line),
    //     app->index++,
    //     uart_demo_submenu_item_callback,
    //     app);
    FURI_LOG_I("UART_DEMO", "%s", furi_string_get_cstr(line));
}
#else
void uart_demo_timer_callback(void *context)
{
    UartDemoApp *app = context;
    FuriString *line = furi_string_alloc();
    while (uart_helper_read(app->uart_helper, line, 0)) {
        submenu_add_item(app->submenu,
                         furi_string_get_cstr(line),
                         app->index++,
                         uart_demo_submenu_item_callback, app);
    }
    furi_string_free(line);
}
#endif

static bool uart_demo_navigation_callback(void *context)
{
    UNUSED(context);
    // We don't want to handle any navigation events, the back button should exit the app.
    return true;
}

static uint32_t uart_demo_exit(void *context)
{
    UNUSED(context);
    // Exit the app.
    return VIEW_NONE;
}


static UartDemoApp *uart_demo_app_alloc()
{
    UartDemoApp *app = malloc(sizeof(UartDemoApp));

    // Initialize the GUI. Create a view dispatcher and attach it to the GUI.
    // Create a submenu, add default entries and add the submenu to the view
    // dispatcher. Set the submenu as the current view.
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui,
                                  ViewDispatcherTypeFullscreen);
    app->submenu = submenu_alloc();
    uart_demo_submenu_add_default_entries(app->submenu, app);
    view_dispatcher_add_view(app->view_dispatcher, UartDemoSubMenuViewId,
                             submenu_get_view(app->submenu));
    view_set_previous_callback(submenu_get_view(app->submenu),
                               uart_demo_exit);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher,
                                                  uart_demo_navigation_callback);
    view_dispatcher_switch_to_view(app->view_dispatcher,
                                   UartDemoSubMenuViewId);

    // Allocate a string to store sendings commands
    app->send_cmd = furi_string_alloc();
    app->lora_bitmask = 0;

    app->msg_response = malloc(sizeof(LoRaMsgResponse));
    app->msg_response->data = malloc(256); // Allocate memory for the data field

    // Initialize the UART helper.
    app->uart_helper = uart_helper_alloc(DEVICE_BAUDRATE);

#ifdef DEMO_PROCESS_LINE
    uart_helper_set_delimiter(app->uart_helper, LINE_DELIMITER,
                              INCLUDE_LINE_DELIMITER);
    uart_helper_set_callback(app->uart_helper, handle_default_response,
                             app);
#else
    app->timer =
        furi_timer_alloc(uart_demo_timer_callback, FuriTimerTypePeriodic,
                         app);
    furi_timer_start(app->timer, 1000);
#endif

    return app;
}

static void uart_demo_app_free(UartDemoApp *app)
{
    if (app->timer) {
        furi_timer_free(app->timer);
    }
    uart_helper_free(app->uart_helper);

    furi_string_free(app->send_cmd);

    view_dispatcher_remove_view(app->view_dispatcher,
                                UartDemoSubMenuViewId);
    view_dispatcher_free(app->view_dispatcher);
    submenu_free(app->submenu);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t uart_demo_main(void *p)
{
    UNUSED(p);

    UartDemoApp *app = uart_demo_app_alloc();
    Expansion *expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(expansion);
    view_dispatcher_run(app->view_dispatcher);
    uart_demo_app_free(app);
    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);
    return 0;
}
