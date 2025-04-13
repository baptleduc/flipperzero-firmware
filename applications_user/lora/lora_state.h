#pragma once


typedef enum {
    INIT,                       // Being on this state means the device is not configured yet
    CONFIG,                     // Being on this state means the device has been configured (e.g. AppKey, DR, TxPower)
    JOINED,                     // Being on this state means the device has joined the network
    SENDING,                    // Being on this state means the device is sending data
    RX,                         // Being on this state means the device is receiving data
} LoraState;
