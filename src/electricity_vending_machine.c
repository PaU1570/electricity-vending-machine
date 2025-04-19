#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "SH1106_I2C.h"
#include "FONT_spleen_8x16.h"
#include "events.h"
#include "pico/util/queue.h"
#include "pico/time.h"
#include "defines.h"

typedef struct {
    uint64_t balance_cents;
    uint64_t balance_wh;
    uint64_t total_energy_wh;
    uint8_t relay_state;
} meter_state_t;

typedef struct {
    meter_state_t left;
    meter_state_t right;
} state_t;

state_t state = {0};
queue_t event_queue;

// for button debounce
#define BUTTON_DEBOUNCE_TIME 25000 // us (25ms)
#define BUTTON_DEBOUNCE_CHECKER_TIME 10000 // us (10ms)
absolute_time_t timeStamp;
repeating_timer_t debounce_timer;

void gpio_callback(uint gpio, uint32_t events);
bool button_debounce_timer(repeating_timer_t *rt);

int main()
{
    stdio_init_all();

    // Initialize the queue
    queue_init(&event_queue, sizeof(event_t), 100);

    // Initialize buttons
    gpio_init(PIN_BUTTON_L_GND);
    gpio_set_dir(PIN_BUTTON_L_GND, GPIO_OUT);
    gpio_put(PIN_BUTTON_L_GND, 0); // Set to ground
    gpio_init(PIN_BUTTON_L);
    gpio_set_dir(PIN_BUTTON_L, GPIO_IN);
    gpio_pull_up(PIN_BUTTON_L);
    gpio_init(PIN_BUTTON_R);
    gpio_set_dir(PIN_BUTTON_R, GPIO_IN);
    gpio_pull_up(PIN_BUTTON_R);

    // Add interrupts
    gpio_set_irq_callback(&gpio_callback);
    gpio_set_irq_enabled(PIN_BUTTON_L, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(PIN_BUTTON_R, GPIO_IRQ_EDGE_FALL, true);
    irq_set_enabled(IO_IRQ_BANK0, true);

    // I2C Initialisation.
    i2c_init(I2C_PORT, SH1106_I2C_CLK*1000);
    
    gpio_set_function(PIN_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_I2C_SDA);
    gpio_pull_up(PIN_I2C_SCL);

    // Power on display
    gpio_init(PIN_I2C_VCC);
    gpio_set_dir(PIN_I2C_VCC, GPIO_OUT);
    gpio_put(PIN_I2C_VCC, 1);
    sleep_ms(100); // Wait for power to stabilize

    // Initialize display
    SH1106_I2C_SetDebug(false);
    SH1106_I2C_Init();
    SH1106_I2C_DrawString("EVM VERSION 0.1", 0, 0, spleen_8x16_FontInfo, 1);
    SH1106_I2C_UpdateDisplay();

    while (true) {
        while (!queue_is_empty(&event_queue)) {
            event_t event;
            queue_remove_blocking(&event_queue, &event);
            switch (event.name) {
                case EVENT_BUTTON_L:
                    // Handle left button press
                    printf("Left button pressed\n");
                    break;
                case EVENT_BUTTON_R:
                    // Handle right button press
                    printf("Right button pressed\n");
                    break;
                case EVENT_COIN_ACCEPTOR:
                    // Handle coin acceptor event
                    break;
                case EVENT_BILL_ACCEPTOR:
                    // Handle bill acceptor event
                    break;
                case EVENT_PZEM_DATA:
                    // Handle PZEM data event
                    break;
                case EVENT_RELAY_1:
                    // Handle relay 1 event
                    break;
                case EVENT_RELAY_2:
                    // Handle relay 2 event
                    break;
                default:
                    break;
            }
        }
        while (queue_is_empty(&event_queue)) {
            sleep_ms(100);
        }
    }
}

void gpio_callback(uint gpio, uint32_t events) {
    // Only buttons and coin/bill acceptors trigger interrupts
    // printf("GPIO callback triggered for GPIO %d\n", gpio);
    if (gpio == PIN_BUTTON_L || gpio == PIN_BUTTON_R) {
        if (absolute_time_diff_us(timeStamp, get_absolute_time()) > BUTTON_DEBOUNCE_TIME) {
            timeStamp = get_absolute_time();
            // printf("Button %d pressed\n", gpio);
            queue_try_add(&event_queue, &(event_t){.name = gpio, .data = 1});
            add_repeating_timer_us(BUTTON_DEBOUNCE_CHECKER_TIME, button_debounce_timer, NULL, &debounce_timer);
        }
    }
    // queue_add_blocking(&event_queue, &(event_t){.name = gpio, .data = 1});
}

bool button_debounce_timer(repeating_timer_t *rt) {
    if (absolute_time_diff_us(timeStamp, get_absolute_time()) < BUTTON_DEBOUNCE_TIME) {
        if (gpio_get(PIN_BUTTON_L) == 0 || gpio_get(PIN_BUTTON_R) == 0) {
            timeStamp = get_absolute_time();
        }
        return true; // Keep the timer running
    }
    return false; // Stop the timer
}