#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include "pico/time.h"
#include "pico/multicore.h"
#include "hardware/i2c.h"
#include "SH1106_I2C.h"
#include "FONT_spleen_8x16.h"
#include "events.h"
#include "defines.h"
#include "pzem-004t_modbus-rtu.h"

typedef struct {
    uint32_t balance_cents;
    uint32_t balance_wh;
    uint8_t relay_state;
} meter_state_t;

typedef struct {
    meter_state_t left;
    meter_state_t right;
    uint32_t price_cents_per_kwh;
    uint32_t price_wh_per_cent;
    uint32_t pending_balance_cents;
    side_t selected;
} state_t;

state_t state = {0};
queue_t event_queue;

// for side selection
#define SIDE_SELECT_TIMEOUT (30 * 1000000) // us (30s)
absolute_time_t side_select_timestamp;

// for button debounce
#define BUTTON_DEBOUNCE_TIME 25000 // us (25ms)
#define BUTTON_DEBOUNCE_CHECKER_TIME 10000 // us (10ms)
absolute_time_t button_timestamp;

// for coin/bill timer (makes sure that event is only triggered if pulse is long enough, prevents interference from relay)
#define COIN_BILL_WAIT_TIME 25000 // us (25ms) (pulses are 50ms long)
absolute_time_t coin_bill_timestamp;

// interrupt handler
void gpio_callback(uint gpio, uint32_t events);
// timers
bool button_debounce_timer_callback(repeating_timer_t *rt);
bool side_select_timer_callback(repeating_timer_t *rt);
bool pzem_polling_timer_callback(repeating_timer_t *rt);
int64_t coin_bill_alarm_callback(alarm_id_t id, void *data);
typedef struct {
    pzem004t_data_t pzem_data;
    uint32_t current_energy;
    side_t side;
} pzem_polling_timer_data_t;

void update_display();
void update_wh_balance();
void check_balance_and_relay();

void core1_main();

int main()
{
    stdio_init_all();

    // Initialize state
    state.price_cents_per_kwh = 1000;
    if (state.price_cents_per_kwh == 0) {
        printf("Error: price per kWh cannot be 0\n");
        return -1;
    } else if (state.price_cents_per_kwh > 1000) {
        printf("Error: price per kWh cannot be higher than 1000\n");
        return -1;
    }
    state.price_wh_per_cent = 1000U / state.price_cents_per_kwh;

    // Initialize the queue
    queue_init(&event_queue, sizeof(event_t), 100);

    // Initialize PZEM-004T
    if (!pzem004t_init(UART_ID_PZEM, PIN_PZEM_TX, PIN_PZEM_RX, false)) {
        printf("Error: PZEM-004T initialization failed\n");
        return -1;
    }
    pzem004t_reset_energy(ADDR_PZEM_L);
    pzem004t_reset_energy(ADDR_PZEM_R);

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

    // Initialize coin/bill acceptor
    gpio_init(PIN_BILL_ACCEPTOR_INHIBIT);
    gpio_set_dir(PIN_BILL_ACCEPTOR_INHIBIT, GPIO_OUT);
    gpio_put(PIN_BILL_ACCEPTOR_INHIBIT, 1); // Set to high to disable bill acceptor
    gpio_init(PIN_BILL_ACCEPTOR);
    gpio_set_function(PIN_BILL_ACCEPTOR, GPIO_FUNC_SIO);
    gpio_set_dir(PIN_BILL_ACCEPTOR, GPIO_IN);
    gpio_pull_up(PIN_BILL_ACCEPTOR);
    gpio_init(PIN_COIN_ACCEPTOR);
    gpio_set_function(PIN_COIN_ACCEPTOR, GPIO_FUNC_SIO);
    gpio_set_dir(PIN_COIN_ACCEPTOR, GPIO_IN);
    gpio_pull_up(PIN_COIN_ACCEPTOR);
    
    // Initialize relay
    gpio_init(PIN_RELAY_L);
    gpio_set_function(PIN_RELAY_L, GPIO_FUNC_SIO);
    gpio_set_dir(PIN_RELAY_L, GPIO_OUT);
    gpio_put(PIN_RELAY_L, RELAY_OFF);
    gpio_init(PIN_RELAY_R);
    gpio_set_function(PIN_RELAY_R, GPIO_FUNC_SIO);
    gpio_set_dir(PIN_RELAY_R, GPIO_OUT);
    gpio_put(PIN_RELAY_R, RELAY_OFF);
    
    // Add interrupts
    gpio_set_irq_callback(&gpio_callback);
    gpio_set_irq_enabled(PIN_BUTTON_L, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(PIN_BUTTON_R, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(PIN_COIN_ACCEPTOR, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(PIN_BILL_ACCEPTOR, GPIO_IRQ_EDGE_FALL, true);
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
    sleep_ms(1000); // Splash screen

    // Start core 1 power monitoring
    multicore_launch_core1(core1_main);

    while (true) {
        while (!queue_is_empty(&event_queue)) {
            event_t event;
            queue_remove_blocking(&event_queue, &event);
            switch (event.name) {
                case EVENT_BUTTON_L: // fallthrough
                case EVENT_BUTTON_R:
                    // Handle left button press
                    printf("%s button pressed\n", (event.name == EVENT_BUTTON_L) ? "Left" : "Right");
                    state.selected = (event.name == EVENT_BUTTON_L) ? LEFT : RIGHT;
                    if (state.selected == LEFT) {
                        state.left.balance_cents += state.pending_balance_cents;
                    } else {
                        state.right.balance_cents += state.pending_balance_cents;
                    }
                    state.pending_balance_cents = 0;

                    gpio_put(PIN_BILL_ACCEPTOR_INHIBIT, 0); // Enable bill acceptor
                    side_select_timestamp = get_absolute_time();
                    static repeating_timer_t side_select_timer;
                    cancel_repeating_timer(&side_select_timer);
                    add_repeating_timer_us(SIDE_SELECT_TIMEOUT, side_select_timer_callback, NULL, &side_select_timer);
                    break;
                case EVENT_COIN_ACCEPTOR:
                    // Handle coin acceptor event
                    switch (state.selected) {
                        case LEFT:
                            state.left.balance_cents += 10;
                            break;
                        case RIGHT:
                            state.right.balance_cents += 10;
                            break;
                        default:
                            state.pending_balance_cents += 10;
                            break;
                    }
                    cancel_repeating_timer(&side_select_timer);
                    add_repeating_timer_us(SIDE_SELECT_TIMEOUT, side_select_timer_callback, NULL, &side_select_timer);
                    break;
                case EVENT_BILL_ACCEPTOR:
                    // Handle bill acceptor event
                    switch (state.selected) {
                        case LEFT:
                            state.left.balance_cents += 500;
                            break;
                        case RIGHT:
                            state.right.balance_cents += 500;
                            break;
                        default:
                            state.pending_balance_cents += 500;
                            break;
                    }
                    cancel_repeating_timer(&side_select_timer);
                    add_repeating_timer_us(SIDE_SELECT_TIMEOUT, side_select_timer_callback, NULL, &side_select_timer);
                    break;
                case EVENT_DECREASE_BALANCE:
                    // Handle decrease balance event
                    if (event.side == LEFT) {
                        if (state.left.balance_cents < event.data) {
                            state.left.balance_cents = 0;
                        } else {
                            state.left.balance_cents -= event.data;
                        }
                    } else if (event.side == RIGHT) {
                        if (state.right.balance_cents < event.data) {
                            state.right.balance_cents = 0;
                        } else {
                            state.right.balance_cents -= event.data;
                        }
                    }
                    break;
                default:
                    break;
            }
        }
        check_balance_and_relay();
        update_display();
        sleep_ms(UPDATE_INTERVAL_MS);  
    }
}

void core1_main() {
    alarm_pool_t *alarm_pool = alarm_pool_create_with_unused_hardware_alarm(1);
    static repeating_timer_t pzem_polling_timer;

    pzem_polling_timer_data_t data_l = {0};
    pzem_polling_timer_data_t data_r = {0};
    data_l.side = LEFT;
    data_r.side = RIGHT;

    alarm_pool_add_repeating_timer_ms(alarm_pool, PZMEM_POLLING_INTERVAL_MS, pzem_polling_timer_callback, (void*)(&data_l), &pzem_polling_timer);
    alarm_pool_add_repeating_timer_ms(alarm_pool, PZMEM_POLLING_INTERVAL_MS, pzem_polling_timer_callback, (void*)(&data_r), &pzem_polling_timer);

    while (true) {
        tight_loop_contents();
    }
}

void gpio_callback(uint gpio, uint32_t events) {
    // Only buttons and coin/bill acceptors trigger interrupts
    printf("GPIO callback triggered for GPIO %d\n", gpio);
    if (gpio == PIN_BUTTON_L || gpio == PIN_BUTTON_R) { // button event
        if (absolute_time_diff_us(button_timestamp, get_absolute_time()) > BUTTON_DEBOUNCE_TIME) {
            button_timestamp = get_absolute_time();
            queue_try_add(&event_queue, &(event_t){.name = gpio});
            static repeating_timer_t button_debounce_timer;
            add_repeating_timer_us(BUTTON_DEBOUNCE_CHECKER_TIME, button_debounce_timer_callback, NULL, &button_debounce_timer);
        }
    }
    else if (gpio == PIN_COIN_ACCEPTOR || gpio == PIN_BILL_ACCEPTOR) { // coin/bill acceptor event
        // If pulse is detected, check after some time if signal is sitll low
        add_alarm_in_us(COIN_BILL_WAIT_TIME, coin_bill_alarm_callback, (void*)(intptr_t)gpio, false);
    }
}

bool button_debounce_timer_callback(repeating_timer_t *rt) {
    if (absolute_time_diff_us(button_timestamp, get_absolute_time()) < BUTTON_DEBOUNCE_TIME) {
        if (gpio_get(PIN_BUTTON_L) == 0 || gpio_get(PIN_BUTTON_R) == 0) {
            button_timestamp = get_absolute_time();
        }
        return true; // Keep the timer running
    }
    return false; // Stop the timer
}

bool side_select_timer_callback(repeating_timer_t *rt) {
    if (absolute_time_diff_us(side_select_timestamp, get_absolute_time()) < SIDE_SELECT_TIMEOUT) {
        return true; // Keep the timer running
    }
    state.selected = NONE;
    gpio_put(PIN_BILL_ACCEPTOR_INHIBIT, 1); // Disable bill acceptor
    return false; // Stop the timer
}

bool pzem_polling_timer_callback(repeating_timer_t *rt) {
    pzem_polling_timer_data_t *data = (pzem_polling_timer_data_t*)(rt->user_data);
    uint32_t prev_energy = data->pzem_data.energy;
    if (pzem004t_read_data((data->side == LEFT) ? ADDR_PZEM_L : ADDR_PZEM_R, &data->pzem_data)) {
        data->current_energy += (data->pzem_data.energy - prev_energy);
        // printf("Power: %d\tCurrent energy: %d\n", data->pzem_data.power, data->current_energy);
        if (data->current_energy >= state.price_wh_per_cent) {
            uint32_t cents = data->current_energy / state.price_wh_per_cent;
            if (queue_try_add(&event_queue, &(event_t){.name = EVENT_DECREASE_BALANCE, .side = data->side, .data = cents})) {
                data->current_energy -= cents * state.price_wh_per_cent;
            }
        }
    } else {
        printf("Error: PZEM-004T read failed\n");
    }
    return true;
}

int64_t coin_bill_alarm_callback(alarm_id_t id, void *data) {
    uint gpio = (uint)data;
    if (gpio_get(gpio) == 0) {
        // Coin or bill detected
        queue_add_blocking(&event_queue, &(event_t){.name = gpio});
    }
    return 0; // No need to repeat
}

void update_display() {
    static uint8_t line_height = 16;
    static char buffer_left[17];
    static char buffer_right[17];
    static char buffer_price[17];

    update_wh_balance();

    if (state.left.balance_wh >= 1000) {
        snprintf(buffer_left, sizeof(buffer_left), "<%05.2f$/%04.1fkWh ", MIN((double)state.left.balance_cents / 100, 99.99), 
        MIN((double)state.left.balance_wh / 1000, 99.9));
    } else {
        snprintf(buffer_left, sizeof(buffer_left), "<%05.2f$/%04.2fkWh ", MIN((double)state.left.balance_cents / 100, 99.99), 
        MIN((double)state.left.balance_wh / 1000, 99.9));
    }
    if (state.right.balance_wh >= 1000) {
        snprintf(buffer_right, sizeof(buffer_right), " %05.2f$/%04.1fkWh>", MIN((double)state.right.balance_cents / 100, 99.99), 
        MIN((double)state.right.balance_wh / 1000, 99.9));
    } else {
        snprintf(buffer_right, sizeof(buffer_right), " %05.2f$/%04.2fkWh>", MIN((double)state.right.balance_cents / 100, 99.99), 
        MIN((double)state.right.balance_wh / 1000, 99.9));
    }
    
    snprintf(buffer_price, sizeof(buffer_price), "Price: %.1f$/kWh", (double)state.price_cents_per_kwh / 100.0);

    SH1106_I2C_ClearScreen();
    SH1106_I2C_DrawString(buffer_left, 0, 0, spleen_8x16_FontInfo, 1);
    SH1106_I2C_DrawString(buffer_right, 0, line_height, spleen_8x16_FontInfo, 1);
    SH1106_I2C_DrawLineHorizontal(0, SH1106_I2C_OLED_MAX_COLUMN, 2*line_height-1, 1);
    switch (state.selected) {
        case LEFT:
            SH1106_I2C_DrawString("< INSERT COIN   ", 0, 2*line_height, spleen_8x16_FontInfo, 1);
            break;
        case RIGHT:
            SH1106_I2C_DrawString("   INSERT COIN >", 0, 2*line_height, spleen_8x16_FontInfo, 1);
            break;
        default:
            SH1106_I2C_DrawString("< SELECT SIDE  >", 0, 2*line_height, spleen_8x16_FontInfo, 1);
            break;
    }
    SH1106_I2C_DrawString(buffer_price, 0, 3*line_height, spleen_8x16_FontInfo, 1);
    SH1106_I2C_UpdateDisplay();
}

void update_wh_balance() {
    // Update the balance in watt-hours
    state.left.balance_wh = (state.left.balance_cents * 1000) / state.price_cents_per_kwh;
    state.right.balance_wh = (state.right.balance_cents * 1000) / state.price_cents_per_kwh;
}

void check_balance_and_relay() {
    gpio_put(PIN_RELAY_L, (state.left.balance_cents > 0) ? RELAY_ON : RELAY_OFF);
    gpio_put(PIN_RELAY_R, (state.right.balance_cents > 0) ? RELAY_ON : RELAY_OFF);
}