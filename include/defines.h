#ifndef DEFINES_H
#define DEFINES_H

// General defines
#define UPDATE_INTERVAL_MS 15
typedef enum {
    NONE = 0,
    RIGHT,
    LEFT
} side_t;

// I2C defines
#define I2C_PORT i2c1
#define PIN_I2C_SDA 26
#define PIN_I2C_SCL 27
#define PIN_I2C_VCC 28

// PZEM-004T defines
// Use pins 0 and 1 for UART0
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define UART_ID_PZEM uart0
#define PIN_PZEM_RX 1
#define PIN_PZEM_TX 0
// Modbus RTU addresses for PZEM-004T. Must be set beforehand one by one using the pzem library.
#define ADDR_PZEM_L 0x01
#define ADDR_PZEM_R 0x02
#define PZMEM_POLLING_INTERVAL_MS 100

// Coin/bill acceptor defines
#define PIN_COIN_ACCEPTOR 5
#define PIN_BILL_ACCEPTOR 18
#define PIN_BILL_ACCEPTOR_INHIBIT 19 // tie to ground to enable bill acceptor

// Relay defines
#define PIN_RELAY_L 17
#define PIN_RELAY_R 16
#define RELAY_ON 1
#define RELAY_OFF 0

// Button defines
#define PIN_BUTTON_L 11
#define PIN_BUTTON_L_GND 12
#define PIN_BUTTON_R 14

#endif