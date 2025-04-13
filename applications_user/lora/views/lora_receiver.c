
#include "lora_receiver_i.h"


/**
 * @brief Convert a hex string to a string.
 * @param hex_str Pointer to the hex string.
 * @param output_str Pointer to the output string.
 * @return void
 */
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

/**
 * Trim everything before and including the given delimiter.
 * Returns -1 if delimiter not found.
 */
static int trim_until_delimiter(FuriString *line, char delimiter)
{
    size_t index = furi_string_search_char(line, delimiter, 0);
    if (index == FURI_STRING_FAILURE) {
        return -1;
    }
    furi_string_right(line, index + 1);
    return 0;
}

// -- PARSING METHODS -----------------------------------------------

static int parse_pending(FuriString *line,
                         LoraMsgResponseModel *msg_response)
{
    if (furi_string_search_str(line, "FPENDING", 0) != FURI_STRING_FAILURE) {
        msg_response->is_pending = true;
        return 0;
    }
    return 1;
}

static int parse_link_info(FuriString *line,
                           LoraMsgResponseModel *msg_response)
{
    size_t index = furi_string_search_str(line, "Link ", 0);
    if (index == FURI_STRING_FAILURE)
        return 1;

    const char *str_start = furi_string_get_cstr(line) + index + 5;
    char *endptr = NULL;
    msg_response->margin = (uint8_t) strtol(str_start, &endptr, 10);

    if (*endptr == ',') {
        msg_response->gateway_count =
            (uint8_t) strtol(endptr + 1, NULL, 10);
    }
    return 0;
}

static int parse_rxwin_info(FuriString *line,
                            LoraMsgResponseModel *msg_response)
{
    size_t index = furi_string_search_str(line, "RXWIN", 0);
    if (index == FURI_STRING_FAILURE)
        return 1;

    msg_response->rx_window =
        (uint8_t) (furi_string_get_char(line, index + 5) - '0');

    const char *str_start = furi_string_get_cstr(line) + index + 14;
    char *endptr = NULL;

    msg_response->rssi = -(int8_t) strtol(str_start, &endptr, 10);
    msg_response->snr = (int8_t) strtol(endptr + 6, NULL, 10);
    return 0;
}

static int parse_ack(FuriString *line, LoraMsgResponseModel *msg_response)
{
    if (furi_string_search_str(line, "ACK Received", 0) !=
        FURI_STRING_FAILURE) {
        msg_response->is_ack = true;
        return 0;
    }
    return 1;
}

static int parse_multicast(FuriString *line,
                           LoraMsgResponseModel *msg_response)
{
    if (furi_string_search_str(line, "MULTICAST", 0) !=
        FURI_STRING_FAILURE) {
        msg_response->is_multicast = true;
        return 0;
    }
    return 1;
}

static int parse_port(FuriString *line, LoraMsgResponseModel *msg_response)
{
    size_t index = furi_string_search_str(line, "PORT: ", 0);
    if (index != FURI_STRING_FAILURE) {
        const char *str_start = furi_string_get_cstr(line) + index + 6;
        msg_response->port = (uint8_t) strtol(str_start, NULL, 10);
    }
    return 0;
}

// Function to parse the RX packet without trimming the input string
static int parse_rx_packet(FuriString *line,
                           LoraMsgResponseModel *rx_response)
{
    if (!line || !rx_response)
        return -1;

    // Check if the line contains "RX"
    if (furi_string_search_str(line, "RX", 0) == FURI_STRING_FAILURE) {
        return 1;
    }

    size_t index = furi_string_search_char(line, '"', 0) + 1; // Skip the first quote

    if (index != FURI_STRING_FAILURE) {
        const char *data_start = furi_string_get_cstr(line) + index;
        strcpy(rx_response->data, data_start);

        size_t length = strlen(rx_response->data);
        if (length > 0 && rx_response->data[length - 2] == '"') {
            rx_response->data[length - 2] = '\0'; // Remove the trailing quote
        }
        return 0;
    }

    return 0;
}

static int parse_msg_response(FuriString *line,
                              LoraMsgResponseModel *msg_response)
{
    if (!line || !msg_response)
        return -1;

    FURI_LOG_D("parse_msg_response", "%s", furi_string_get_cstr(line));

    if (trim_until_delimiter(line, ':') < 0) {
        return -1;              // Delimiter not found
    }
    // Try each parser until one succeeds
    if (parse_pending(line, msg_response) == 0)
        return 0;
    if (parse_link_info(line, msg_response) == 0)
        return 0;
    if (parse_rxwin_info(line, msg_response) == 0)
        return 0;
    if (parse_ack(line, msg_response) == 0)
        return 0;
    if (parse_multicast(line, msg_response) == 0)
        return 0;
    if (parse_rx_packet(line, msg_response) == 0) {
        if (parse_port(line, msg_response) == 0) { // Port and Data are in the same line in MSG
            return 0;
        }
    }
    // If none matched, fallback to RX packet
    return 1;
}

/**
 * @brief Set the data message response and update the view.
 * @param receiver Pointer to the LoraReceiver object.
 * @param data Pointer to the data to be set.
 */
void lora_receiver_decode_msg_response(LoraReceiver *receiver,
                                       FuriString *line)
{
    furi_assert(receiver);
    furi_assert(line);

    with_view_model(receiver->view, LoraReceiverModel * model, {
                    parse_msg_response(line, &model->msg_response);
                    hex_to_string(model->msg_response.data,
                                  model->msg_response.decoded_data);},
                    true);
}

static void lora_receiver_draw_callback(Canvas *canvas, void *_model)
{

    LoraReceiverModel *model = _model;
    char temp_str[18];

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, "Freq: %d MHz");
    canvas_draw_str(canvas, 2, 20, "RSSI: ");

    snprintf(temp_str, 18, "SNR: %d dB", model->msg_response.snr);
    canvas_draw_str_aligned(canvas, 126, 20, AlignRight, AlignBottom,
                            temp_str);


}

LoraReceiver *lora_receiver_alloc(void)
{
    LoraReceiver *receiver = malloc(sizeof(LoraReceiver));
    furi_assert(receiver);

    receiver->view = view_alloc();
    view_allocate_model(receiver->view, ViewModelTypeLocking,
                        sizeof(LoraReceiverModel));


    view_set_context(receiver->view, receiver);
    view_set_draw_callback(receiver->view, lora_receiver_draw_callback);
    return receiver;
}

void lora_receiver_free(LoraReceiver *receiver)
{
    furi_assert(receiver);
    view_free_model(receiver->view);
    view_free(receiver->view);
    free(receiver);
}

View *lora_receiver_get_view(LoraReceiver *receiver)
{
    furi_assert(receiver);
    return receiver->view;
}
