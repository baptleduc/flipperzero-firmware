#include "lora_app.h"
#include "lora_parsers.h"

// Global variables
int dr = DEFAULT_DR;            // Data rate
int tx_power = DEFAULT_TX_POWER; // Transmit power

/**
 * This callback function is called when a submenu item is clicked.
 * 
 * @param context The context passed to the submenu.
 * @param index   The index of the submenu item that was clicked.
*/
static void lora_submenu_item_callback(void *context, uint32_t index);

/**
 * Adds the default submenu entries.
 * 
 * @param submenu The submenu.
 * @param context The context to pass to the submenu item callback function.
*/
static void lora_submenu_add_default_entries(Submenu *submenu,
                                             void *context)
{
    LoraApp *app = context;
    submenu_reset(submenu);
    submenu_add_item(submenu, "Clear", 0, lora_submenu_item_callback,
                     context);
    submenu_add_item(submenu, "OTAA join", 1, lora_submenu_item_callback,
                     context);
    submenu_add_item(submenu, "Reiceive Mode", 2,
                     lora_submenu_item_callback, context);
    submenu_add_item(submenu, "Send Msg 2", 3, lora_submenu_item_callback,
                     context);

    app->index = 3;
}

static void otaa_join_procedure(void *context)
{
    LoraApp *app = context;
    int join_attempts = 1;

    // Loop until the device is joined to the network
    while (app->current_state != JOINED) {
        FURI_LOG_I("OTAA_JOIN", "Attempting to join the network...");
        // Send the join command
        uart_helper_send(app->uart_helper, "AT+JOIN_CMD\n", 13);
        furi_delay_ms(10000);

        if (app->current_state == JOINED) {
            // Device is joined, break the loop
            break;
        }

        join_attempts++;

        if (join_attempts > 3) {
            // If the device is not joined after 3 attempts, break the loop
            FURI_LOG_I("OTAA_JOIN", "Failed to join after 3 attempts");
            break;
        }

        FURI_LOG_I("OTAA_JOIN", "Join attempt %d", join_attempts);
    }
}

static void setup_lora_connexion(void *context)
{
    LoraApp *app = context;

    uart_helper_send(app->uart_helper, "AT+ID\n", 7);
    furi_delay_ms(1000);

    uart_helper_send(app->uart_helper, "AT+MODE=LWOTAA\n", 16);
    furi_delay_ms(1000);

    furi_string_printf(app->send_cmd, "AT+DR=%d\n", dr);
    uart_helper_send_string(app->uart_helper, app->send_cmd);
    furi_delay_ms(1000);

    furi_string_printf(app->send_cmd, "AT+POWER=%d\n", tx_power);
    uart_helper_send_string(app->uart_helper, app->send_cmd);
    furi_delay_ms(1000);

    uart_helper_send(app->uart_helper, "AT+ADR=ON\n", 11);
    furi_delay_ms(1000);

    uart_helper_send(app->uart_helper, "AT+CLASS=A\n", 12);
    furi_delay_ms(1000);

    furi_string_printf(app->send_cmd, "AT+KEY=APPKEY,%s\n", APPKEY);
    uart_helper_send_string(app->uart_helper, app->send_cmd);
    furi_delay_ms(1000);

    app->current_state = CONFIG;
}

// static void decode_data(char *data)
// {
//     // Decode the data received from the server
//     // This function is a placeholder and should be implemented according to the specific requirements
//     FURI_LOG_I("DECODE_DATA", "Data: %s", data);
// }
static void send_cmsg(LoraApp *app, const char *msg)
{
    furi_string_printf(app->send_cmd, "AT+CMSG=%s\n", msg);
    uart_helper_send_string(app->uart_helper, app->send_cmd);
    furi_delay_ms(1000);
    DEBUG_LORA_MSG_RESPONSE(*app->msg_response);
}

static void enter_test_mode(void *context)
{
    LoraApp *app = context;
    app->current_state = CONFIG;
    uart_helper_send(app->uart_helper, "AT+MODE=TEST\n", 14);
    furi_delay_ms(1000);
}

static void enter_rx_mode(void *context)
{
    LoraApp *app = context;
    app->current_state = CONFIG;
    enter_test_mode(app);
    uart_helper_send(app->uart_helper, "AT+TEST=RXLRPKT\n", 17);
    furi_delay_ms(1000);
    app->current_state = RX;
}

static void lora_submenu_item_callback(void *context, uint32_t index)
{
    LoraApp *app = context;

    switch (index) {
    case 0:
        // Clear the submenu and add the default entries.
        lora_submenu_add_default_entries(app->submenu, app);
        break;
    case 1:
        setup_lora_connexion(app);
        otaa_join_procedure(app);
        break;
    case 2:
        // Enter RX mode.
        enter_rx_mode(app);
        break;
    case 3:
        // Send a confirmed message to the server.
        send_cmsg(app, "Hello World");
        break;
    default:
        break;
    }
}

// Function to convert hex string to ASCII string
static void hex_to_string(char *hex_str, char *output_str)
{
    size_t len = strlen(hex_str);

    if (len % 2 != 0) {
        printf("Error: Hex string length must be even.\n");
        return;
    }

    for (size_t i = 0; i < len; i += 2) {
        char hex_pair[3] = { hex_str[i], hex_str[i + 1], '\0' };
        output_str[i / 2] = (char) strtol(hex_pair, NULL, 16);
    }

    output_str[len / 2] = '\0'; // Null-terminate the string
}


// -- HANDLERS ---------------------------------------------------------

void handle_msg_response(FuriString *line, void *context)
{
    LoraApp *app = (LoraApp *) context;
    parse_msg_response(line, app->msg_response);
}

void handle_rx_response(FuriString *line, void *context)
{
    LoraApp *app = (LoraApp *) context;
    parse_msg_response(line, app->msg_response);
    hex_to_string(app->msg_response->data,
                  app->msg_response->decoded_data);
    FURI_LOG_I("handle_rx_response", "Decoded data: %s",
               app->msg_response->decoded_data);
}

void handle_join_response(FuriString *line, void *context)
{
    FURI_LOG_I("handle_join_response", "%s", furi_string_get_cstr(line));
    LoraApp *app = context;
    if (furi_string_start_with(line, "+JOIN_CMD: Network joined")) {
        app->current_state = JOINED;
        FURI_LOG_I("handle_join_response", "Network joined");
        return;
    }
}

#ifdef DEMO_PROCESS_LINE
void handle_default_response(FuriString *line, void *context)
{
    (void) context;
    FURI_LOG_I("handle_default_response", "%s",
               furi_string_get_cstr(line));
}
#else
void lora_timer_callback(void *context)
{
    LoraApp *app = context;
    FuriString *line = furi_string_alloc();
    while (uart_helper_read(app->uart_helper, line, 0)) {
        submenu_add_item(app->submenu,
                         furi_string_get_cstr(line),
                         app->index++, lora_submenu_item_callback, app);
    }
    furi_string_free(line);
}
#endif

static bool lora_navigation_callback(void *context)
{
    UNUSED(context);
    // We don't want to handle any navigation events, the back button should exit the app.
    return true;
}

static uint32_t lora_exit(void *context)
{
    UNUSED(context);
    // Exit the app.
    return VIEW_NONE;
}

static LoraApp *lora_app_alloc()
{
    LoraApp *app = malloc(sizeof(LoraApp));

    // Initialize the GUI. Create a view dispatcher and attach it to the GUI.
    // Create a submenu, add default entries and add the submenu to the view
    // dispatcher. Set the submenu as the current view.
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui,
                                  ViewDispatcherTypeFullscreen);
    app->submenu = submenu_alloc();
    lora_submenu_add_default_entries(app->submenu, app);
    view_dispatcher_add_view(app->view_dispatcher, UartDemoSubMenuViewId,
                             submenu_get_view(app->submenu));
    view_set_previous_callback(submenu_get_view(app->submenu), lora_exit);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher,
                                                  lora_navigation_callback);
    view_dispatcher_switch_to_view(app->view_dispatcher,
                                   UartDemoSubMenuViewId);

    // Allocate a string to store sendings commands
    app->send_cmd = furi_string_alloc();
    app->current_state = INIT;

    app->msg_response = malloc(sizeof(LoRaMsgResponse));
    // Initialize the UART helper.
    app->uart_helper = uart_helper_alloc(DEVICE_BAUDRATE);

#ifdef DEMO_PROCESS_LINE
    uart_helper_set_delimiter(app->uart_helper, LINE_DELIMITER,
                              INCLUDE_LINE_DELIMITER);
    uart_helper_set_callback(app->uart_helper, app);
#else
    app->timer =
        furi_timer_alloc(uart_demo_timer_callback, FuriTimerTypePeriodic,
                         app);
    furi_timer_start(app->timer, 1000);
#endif

    return app;
}

static void lora_app_free(LoraApp *app)
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

int32_t lora_app_main(void *p)
{
    UNUSED(p);

    LoraApp *app = lora_app_alloc();
    Expansion *expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(expansion);
    view_dispatcher_run(app->view_dispatcher);
    lora_app_free(app);
    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);
    return 0;
}
