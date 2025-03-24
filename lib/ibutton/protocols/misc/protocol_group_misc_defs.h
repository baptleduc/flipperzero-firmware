#pragma once

#include <protocols/protocol.h>

typedef enum {
    iButtonProtocolMiscCyfral,
    iButtonProtocolMiscMetakom,
    iButtonProtocolMiscMax,
} iButtonProtocolMisc;

extern const ProtocolBase* ibutton_protocols_misc[];
