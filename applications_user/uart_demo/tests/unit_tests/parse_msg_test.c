#include "../../uart_demo.h"

#include <furi.h>

// Test 1: FPENDING message: +MSG: FPENDING
static void parse_msg_with_fpending_test()
{
    FuriString* line = furi_string_alloc();
    furi_string_set(line, "+MSG: FPENDING");
    LoRaMsgResponse msg_response = {0};
    int result = parse_msg_response(line, &msg_response);

    if (result != 0) {
        FURI_LOG_E("parse_msg_with_fpending_test", "parse_msg_response() failed");
    }
    if (msg_response.is_pending != true) {
        FURI_LOG_E("parse_msg_with_fpending_test", "FPENDING not detected");
    }
    
    furi_string_free(line);
}
// Test 2: Link margin and gateway_count message: +MSG: Link 20, 1
static void parse_msg_with_link_test(){
    FuriString* line = furi_string_alloc();
    furi_string_set(line, "+MSG: Link 20, 1");
    LoRaMsgResponse msg_response = {0};
    int result = parse_msg_response(line, &msg_response);
    if (result != 0) {
        FURI_LOG_E("parse_msg_with_link_test", "parse_msg_response() failed");
    }
    if (msg_response.margin != 20) {
        FURI_LOG_E("parse_msg_with_link_test", "Incorrect margin: expected: 20, got: %d", msg_response.margin);
    }
    if (msg_response.gateway_count != 1) {
        FURI_LOG_E("parse_msg_with_link_test", "Incorrect gateway count: expected: 1, got: %d", msg_response.gateway_count);
    }
    
    furi_string_free(line);
}

// Test 3: RXWIN message with RSSI and SNR : +MSG: RXWIN2, RSSI -106, SNR 4
static void parse_msg_with_rxwin_test(){
    FuriString* line = furi_string_alloc();
    furi_string_set(line, "+MSG: RXWIN2, RSSI -106, SNR 4");
    LoRaMsgResponse msg_response = {0};
    int result = parse_msg_response(line, &msg_response);
    
    if (result != 0) {
        FURI_LOG_E("parse_msg_with_rxwin_test", "parse_msg_response() failed");
    }
    if (msg_response.rx_window != 2) {
        FURI_LOG_E("parse_msg_with_rxwin_test", "Incorrect RXWIN: expected: 2, got: %d", msg_response.rx_window);
    }
    if (msg_response.rssi != -106) {
        FURI_LOG_E("parse_msg_with_rxwin_test", "Incorrect RSSI: expected: -106, got: %d", msg_response.rssi);
    }

    if (msg_response.snr != 4) {
        FURI_LOG_E("parse_msg_with_rxwin_test", "Incorrect SNR: expected: 4, got: %d", msg_response.snr);
    }
    
    furi_string_free(line);
}

// Test 4: ACK Received message of type : +MSG: ACK Received
static void parse_msg_with_ack_received_test()
{
    FuriString* line = furi_string_alloc();
    furi_string_set(line, "+MSG: ACK Received");
    LoRaMsgResponse msg_response = {0};
    int result = parse_msg_response(line, &msg_response);
    if (result != 0) {
        FURI_LOG_E("parse_msg_with_ack_received_test", "parse_msg_response() failed");
    }
    if (msg_response.is_ack != true) {
        FURI_LOG_E("parse_msg_with_ack_received_test", "Incorrect ACK status, expected: true, got: %s", msg_response.is_ack ? "true" : "false");
    }

    furi_string_free(line);
}

// Test 5: Test parsing function with message of type : +MSG: PORT: 8; RX: "12345678"
static void parse_msg_with_port_and_data(){
    FuriString* line = furi_string_alloc();
    furi_string_set(line, "+MSG: PORT: 8; RX: \"12345678\"");
    LoRaMsgResponse msg_response = {0};
    int result = parse_msg_response(line, &msg_response);

    if (result != 0) {
        FURI_LOG_E("parse_msg_with_port_and_data", "parse_msg_response() failed");
    }
    if (strcmp(msg_response.data, "12345678") != 0) {
        FURI_LOG_E("parse_msg_with_port_and_data", "Incorrect RX data: expected: 12345678, got: %s", msg_response.data);

    }
    if (msg_response.port != 8) {
        FURI_LOG_E("parse_msg_with_port_and_data", "Incorrect port: expected: 8, got: %d", msg_response.port);
    }
    
    furi_string_free(line);
}

static void parse_msg_with_multicast(){
    FuriString* line = furi_string_alloc();
    furi_string_set(line, "+MSG: MULTICAST");
    LoRaMsgResponse msg_response = {0};
    int result = parse_msg_response(line, &msg_response);
    if (result != 0) {
        FURI_LOG_E("parse_msg_with_multicast", "parse_msg_response() failed");
    }
    if (msg_response.is_multicast != true) {
        FURI_LOG_E("parse_msg_with_multicast", "Incorrect multicast status, expected: true, got: %s", msg_response.is_multicast ? "true" : "false");
    }

    furi_string_free(line);
}



int parse_msg_test_suite()
{
    parse_msg_with_fpending_test();
    parse_msg_with_link_test();
    parse_msg_with_rxwin_test();
    parse_msg_with_ack_received_test();
    parse_msg_with_port_and_data();
    parse_msg_with_multicast();

    FURI_LOG_I("parse_msg_test_suite", "All Unit Tests passed");
    return 0;
}



