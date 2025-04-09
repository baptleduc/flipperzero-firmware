#ifndef LORA_PARSERS_H
#define LORA_PARSERS_H


#include <furi_hal.h>

#include "lora_app.h"

int parse_msg_response(FuriString * line, LoRaMsgResponse * msg_response);

#endif
