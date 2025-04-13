#pragma once

#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct LoraReceiver LoraReceiver;

/**
 * @brief Allocate and initialize a LoraReceiver object.
 * @return Pointer to the allocated LoraReceiver object.
 */
    LoraReceiver *lora_receiver_alloc(void);

/**
 * @brief Free the memory allocated for a LoraReceiver object.
 * @param receiver Pointer to the LoraReceiver object to be freed.
 */
    void lora_receiver_free(LoraReceiver * receiver);

/**
 * @brief Get the view associated with the LoraReceiver object.
 * @param receiver Pointer to the LoraReceiver object.
 * @return Pointer to the View associated with the LoraReceiver object.
 */
    View *lora_receiver_get_view(LoraReceiver * receiver);

// METHODS
    void lora_receiver_set_data_msg_response(LoraReceiver * receiver,
                                             uint8_t * data,
                                             uint32_t size);

/**
 * @brief Parse and decode the raw data received from the LoRa module. Stores the result in
 * in the decoded_data field of the msg_response model.
 * @param receiver Pointer to the LoraReceiver object.
 * @param line Pointer to the FuriString object containing the raw data.
 * 
 * @return void
 */
    void lora_receiver_decode_msg_response(LoraReceiver * receiver,
                                           FuriString * line);

#ifdef __cplusplus
}
#endif
