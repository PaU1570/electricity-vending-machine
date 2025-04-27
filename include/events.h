#ifndef EVENTS_H
#define EVENTS_H

#include "defines.h"

typedef enum {
    EVENT_NONE = 0,
    EVENT_DECREASE_BALANCE,
    EVENT_BUTTON_L = PIN_BUTTON_L,
    EVENT_BUTTON_R = PIN_BUTTON_R,
    EVENT_COIN_ACCEPTOR = PIN_COIN_ACCEPTOR,
    EVENT_BILL_ACCEPTOR = PIN_BILL_ACCEPTOR
} event_name_t;

typedef struct {
    event_name_t name;
    side_t side;
    uint32_t data;
} event_t;

#endif