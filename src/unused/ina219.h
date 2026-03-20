#ifndef INA219_H
#define INA219_H

#include <stdint.h>
#include <stdbool.h>

#include "hardware_config.h"
#define INA219_I2C_ADDR  INA219_ADDR
#define INA219_SDA_PIN   PIN_INA219_SDA
#define INA219_SCL_PIN   PIN_INA219_SCL

typedef struct {
    uint16_t voltage_mv;    // Bus voltage in millivolts
    int16_t  current_ma;    // Current in milliamps (signed)
    uint16_t power_mw;      // Power in milliwatts
    bool     present;       // INA219 detected on I2C
} ina219_data_t;

// Initialize I2C and configure INA219
// Returns false if INA219 not found
bool ina219_init(void);

// Read voltage, current, power
void ina219_read(ina219_data_t *data);

#endif
