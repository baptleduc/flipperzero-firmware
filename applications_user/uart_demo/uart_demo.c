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
    (void) context;

}

static void setup_lora_connexion(void *context)
{
    UartDemoApp *app = context;

    uart_helper_send(app->uart_helper, "AT+ID\n", 7, FTD_MSG);
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
        furi_string_printf(app->send_cmd, "Index is %ld.\n", app->index);
        uart_helper_send_string(app->uart_helper, app->send_cmd, FTD_CMG);
    } else {
        // The item was received data.
    }
}

void handle_msg_response(FuriString *line, void *context)
{
    (void) context;
    FURI_LOG_I("handle_msg_response", "Line: %s",
               furi_string_get_cstr(line));
}

void handle_cmsg_response(FuriString *line, void *context)
{
    (void) context;
    FURI_LOG_I("handle_cmsg_response", "Line: %s",
               furi_string_get_cstr(line));
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
    FURI_LOG_I("UART_DEMO", "Line: %s", furi_string_get_cstr(line));
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
