#include "lora_app.h"
#include "scenes/lora_scene.h"

#include <gui/modules/submenu.h>

// Global variables
int dr = DEFAULT_DR;            // Data rate
int tx_power = DEFAULT_TX_POWER; // Transmit power

void otaa_join_procedure(void *context)
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

void setup_lora_connexion(void *context)
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

    lora_app_set_state(app, CONFIG);
}

void send_cmsg(LoraApp *app, const char *msg)
{
    furi_string_printf(app->send_cmd, "AT+CMSG=%s\n", msg);
    uart_helper_send_string(app->uart_helper, app->send_cmd);
    furi_delay_ms(1000);
}

static void enter_test_mode(void *context)
{
    LoraApp *app = context;
    lora_app_set_state(app, CONFIG);
    uart_helper_send(app->uart_helper, "AT+MODE=TEST\n", 14);
    furi_delay_ms(1000);
}

void lora_enter_receive_mode(void *context)
{
    LoraApp *app = context;
    lora_app_set_state(app, CONFIG);
    enter_test_mode(app);
    uart_helper_send(app->uart_helper, "AT+TEST=RXLRPKT\n", 17);
    furi_delay_ms(1000);
    lora_app_set_state(app, RX);
}



// -- HANDLERS ---------------------------------------------------------



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

static bool lora_app_custom_event_callback(void *context, uint32_t event)
{
    furi_assert(context);
    LoraApp *app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool lora_app_back_event_callback(void *context)
{
    furi_assert(context);
    LoraApp *app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static LoraApp *lora_app_alloc()
{
    LoraApp *app = malloc(sizeof(LoraApp));

    // Initialize the GUI. Create a view dispatcher and attach it to the GUI.
    // Create a submenu, add default entries and add the submenu to the view
    // dispatcher. Set the submenu as the current view.
    app->gui = furi_record_open(RECORD_GUI);
    app->scene_manager = scene_manager_alloc(&lora_scene_handlers, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher,
                                              lora_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher,
                                                  lora_app_back_event_callback);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui,
                                  ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, LoraAppSubMenuView,
                             submenu_get_view(app->submenu));

    // Allocate a receiver object
    app->receiver = lora_receiver_alloc();
    view_dispatcher_add_view(app->view_dispatcher, LoraAppReceiverView,
                             lora_receiver_get_view(app->receiver));

    // Allocate a string to store sendings commands
    app->send_cmd = furi_string_alloc();
    lora_app_set_state(app, INIT);

    // Initialize the UART helper.
    app->uart_helper = uart_helper_alloc(DEVICE_BAUDRATE, app);

#ifdef DEMO_PROCESS_LINE
    uart_helper_set_delimiter(app->uart_helper, LINE_DELIMITER,
                              INCLUDE_LINE_DELIMITER);
#else
    app->timer =
        furi_timer_alloc(uart_demo_timer_callback, FuriTimerTypePeriodic,
                         app);
    furi_timer_start(app->timer, 1000);
#endif
    scene_manager_next_scene(app->scene_manager, LoraSceneStart);
    return app;
}

void lora_app_set_state(LoraApp *app, LoraState state)
{
    furi_assert(app);
    if (app->current_state == state) {
        return;
    }
    lora_receiver_update_process_callback(app->receiver, state);
    app->current_state = state;

}

static void lora_app_free(LoraApp *app)
{
    if (app->timer) {
        furi_timer_free(app->timer);
    }
    uart_helper_free(app->uart_helper);

    furi_string_free(app->send_cmd);

    view_dispatcher_remove_view(app->view_dispatcher, LoraAppSubMenuView);
    view_dispatcher_remove_view(app->view_dispatcher, LoraAppReceiverView);
    view_dispatcher_free(app->view_dispatcher);
    submenu_free(app->submenu);
    lora_receiver_free(app->receiver);
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

    // Free the resources
    lora_app_free(app);
    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);
    return 0;
}
