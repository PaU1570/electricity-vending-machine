#ifndef EVENTS_H
#define EVENTS_H

#include "defines.h"

typedef enum {
    EVENT_NONE = 0,
    EVENT_PZEM_DATA,
    EVENT_BUTTON_L = PIN_BUTTON_L,
    EVENT_BUTTON_R = PIN_BUTTON_R,
    EVENT_COIN_ACCEPTOR = PIN_COIN_ACCEPTOR,
    EVENT_BILL_ACCEPTOR = PIN_BILL_ACCEPTOR,
    EVENT_RELAY_L = PIN_RELAY_L,
    EVENT_RELAY_R = PIN_RELAY_R
} event_name_t;

typedef struct {
    event_name_t name;
    uint8_t data;
} event_t;

#endif