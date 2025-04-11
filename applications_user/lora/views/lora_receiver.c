#include "lora_receiver.h"

LoraReceiver *lora_receiver_alloc()
{
    LoraReceiver *receiver = malloc(sizeof(LoraReceiver));
    furi_assert(receiver);

    receiver->view = view_alloc();
    receiver->msg_response = malloc(sizeof(LoRaMsgResponseModel));
    return receiver;
}

void lora_receiver_free(LoraReceiver *receiver)
{
    furi_assert(receiver);
    free(receiver->msg_response);
    view_free(receiver->view);
    free(receiver);
}
