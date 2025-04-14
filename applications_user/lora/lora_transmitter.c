#include "lora_transmitter_i.h"
#include "lora_app.h"


static void lora_transmitter_init_cfg_model(void *context)
{
    furi_assert(context);
    LoraTransmitter *transmitter = context;
    transmitter->model->cfg.baudrate = DEVICE_BAUDRATE;
    transmitter->model->cfg.dr = DEFAULT_DR;
    transmitter->model->cfg.tx_power = DEFAULT_TX_POWER;
    transmitter->model->cfg.port = PORT;
    strncpy(transmitter->model->cfg.appkey, APPKEY,
            sizeof(transmitter->model->cfg.appkey));
}

static void lora_transmitter_init(void *context)
{
    furi_assert(context);
    lora_transmitter_init_cfg_model(context);
}

LoraTransmitter *lora_transmitter_alloc(void *context,
                                        LoraTransmitterMethod send_method,
                                        LoraTransmitterContextDestructor
                                        context_destructor)
{
    furi_assert(context);
    LoraTransmitter *transmitter = malloc(sizeof(LoraTransmitter));
    transmitter->model = malloc(sizeof(LoraTransmitterModel));
    // Allocate a string to store sendings commands
    transmitter->send_cmd = furi_string_alloc();
    transmitter->context = context;
    transmitter->send_method = send_method;
    transmitter->context_destructor = context_destructor;

    // Init
    lora_transmitter_init(transmitter);
    return transmitter;
}

void lora_transmitter_free(LoraTransmitter *transmitter)
{
    transmitter->context_destructor(transmitter->context);
    free(transmitter->model);
    furi_string_free(transmitter->send_cmd);
    free(transmitter);
}

static void lora_transmitter_enter_test_mode(LoraTransmitter *transmitter)
{
    lora_state_manager_set_state(transmitter->state_manager, CONFIG);
    transmitter->send_method(transmitter->context, "AT+MODE=TEST\n", 14);
    furi_delay_ms(1000);
}

void lora_transmitter_enter_receive_mode(LoraTransmitter *transmitter)
{
    lora_state_manager_set_state(transmitter->state_manager, CONFIG);
    lora_transmitter_enter_test_mode(transmitter);
    transmitter->send_method(transmitter->context, "AT+TEST=RXLRPKT\n",
                             17);
    furi_delay_ms(1000);
    lora_state_manager_set_state(transmitter->state_manager, RX);
}

void lora_transmitter_set_state_manager(LoraTransmitter *transmitter,
                                        LoraStateManager *state_manager)
{
    furi_assert(transmitter);
    furi_assert(state_manager);
    transmitter->state_manager = state_manager;
}

void otaa_join_procedure(LoraTransmitter *transmitter)
{

    int join_attempts = 1;

    // Loop until the device is joined to the network
    while (lora_state_manager_get_state(transmitter->state_manager) !=
           JOINED) {
        FURI_LOG_I("OTAA_JOIN", "Attempting to join the network...");
        // Send the join command
        transmitter->send_method(transmitter->context, "AT+JOIN_CMD\n",
                                 13);
        furi_delay_ms(10000);

        if ((lora_state_manager_get_state(transmitter->state_manager)) ==
            JOINED) {
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

void lora_transmitter_setup_lorawan(LoraTransmitter *transmitter)
{
    UartHelper *uart_helper = transmitter->context;

    transmitter->send_method(transmitter->context, "AT+ID\n", 7);
    furi_delay_ms(1000);

    transmitter->send_method(transmitter->context, "AT+MODE=LWOTAA\n", 16);
    furi_delay_ms(1000);

    furi_string_printf(transmitter->send_cmd, "AT+DR=%d\n",
                       transmitter->model->cfg.dr);
    uart_helper_send_string(uart_helper, transmitter->send_cmd);
    furi_delay_ms(1000);

    furi_string_printf(transmitter->send_cmd, "AT+POWER=%d\n",
                       transmitter->model->cfg.tx_power);
    uart_helper_send_string(uart_helper, transmitter->send_cmd);
    furi_delay_ms(1000);

    transmitter->send_method(transmitter->context, "AT+ADR=ON\n", 11);
    furi_delay_ms(1000);

    transmitter->send_method(transmitter->context, "AT+CLASS=A\n", 12);
    furi_delay_ms(1000);

    furi_string_printf(transmitter->send_cmd, "AT+KEY=APPKEY,%s\n",
                       transmitter->model->cfg.appkey);
    uart_helper_send_string(uart_helper, transmitter->send_cmd);
    furi_delay_ms(1000);

    lora_state_manager_set_state(transmitter->state_manager, CONFIG);
}

void send_cmsg(LoraTransmitter *transmitter, const char *msg)
{
    UartHelper *uart_helper = transmitter->context;
    furi_string_printf(transmitter->send_cmd, "AT+CMSG=%s\n", msg);
    uart_helper_send_string(uart_helper, transmitter->send_cmd);
    furi_delay_ms(1000);
}
