#pragma once

#include "lora_transmitter.h"

#define DEFAULT_DR       5
#define DEFAULT_TX_POWER 14
#define APPKEY           "2FFA5393E446D6CEC8EB9D9AFA5521F3"
#define PORT             10

/**
 * @brief LoRa Transmitter configuration Model
 */
typedef struct {
    uint8_t dr;                 // Data rate
    uint32_t baudrate;          // Baudrate
    uint8_t tx_power;           // Transmit power
    uint8_t port;               // Port number
    char appkey[33];            // AppKey
} LoraTransmitterCfgModel;

/**
 * @brief Lora Transmitter Model
 */
typedef struct {
    LoraTransmitterCfgModel cfg; // Configuration model
    // Possibly other models can be added here in the future
} LoraTransmitterModel;



/**
 * @brief LoRa Transmitter Object
 */
struct LoraTransmitter {
    void *context;
    LoraStateManager *state_manager;
    FuriString *send_cmd;
    LoraTransmitterMethod send_method;
    LoraTransmitterModel *model;
    LoraTransmitterContextDestructor context_destructor;


};
