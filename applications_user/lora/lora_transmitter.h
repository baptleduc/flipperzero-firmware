#pragma once

#include "lora_state_manager.h"
#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif


    typedef struct LoraTransmitter LoraTransmitter;

/**
 * @brief Callback function for sending data
 */
    typedef void (*LoraTransmitterMethod)(void *context, const char *data,
                                          size_t length);

/**
 * @brief Context Destructor
 */
    typedef void (*LoraTransmitterContextDestructor)(void *context);
/**
 * @brief LoRa Transmitter Constructor
 * @return 
 */
    LoraTransmitter *lora_transmitter_alloc(void *context,
                                            LoraTransmitterMethod
                                            send_method,
                                            LoraTransmitterContextDestructor
                                            context_destructor);

/**
 * @brief LoRa Transmitter Destructor
 */
    void lora_transmitter_free(LoraTransmitter * transmitter);

    void lora_transmitter_enter_receive_mode(LoraTransmitter *
                                             transmitter);

// void lora_transmitter_send_msg(LoraTransmitter *transmitter, const char *msg);

    void lora_transmitter_set_state_manager(LoraTransmitter * transmitter,
                                            LoraStateManager *
                                            state_manager);
#ifdef __cplusplus
}
#endif
