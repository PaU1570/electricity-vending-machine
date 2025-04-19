#ifndef DEFINES_H
#define DEFINES_H

// I2C defines
#define I2C_PORT i2c1
#define PIN_I2C_SDA 26
#define PIN_I2C_SCL 27
#define PIN_I2C_VCC 28

// PZEM-004T defines
#define PIN_PZEM_RX 0
#define PIN_PZEM_TX 1

// Coin/bill acceptor defines
#define PIN_COIN_ACCEPTOR 7
#define PIN_BILL_ACCEPTOR 20
#define PIN_BILL_ACCEPTOR_INHIBIT 21 // tie to ground to enable bill acceptor

// Relay defines
#define PIN_RELAY_1 17
#define PIN_RELAY_2 22

// Button defines
#define PIN_BUTTON_L 11
#define PIN_BUTTON_L_GND 12
#define PIN_BUTTON_R 14

#endif