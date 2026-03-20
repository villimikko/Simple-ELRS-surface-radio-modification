#include "ina219.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"

#define INA219_I2C     i2c0

// INA219 registers
#define REG_CONFIG     0x00
#define REG_SHUNT_V    0x01
#define REG_BUS_V      0x02
#define REG_POWER      0x03
#define REG_CURRENT    0x04
#define REG_CALIBRATE  0x05

// 0.1 ohm shunt resistor (typical AliExpress module)
#define SHUNT_RESISTANCE_MOHM  100  // milliohms

static bool ina219_present = false;

// Use timeout variants to avoid hanging when no device is connected
#define I2C_TIMEOUT_US  50000  // 50ms timeout

static bool write_reg(uint8_t reg, uint16_t value) {
    uint8_t buf[3] = { reg, (uint8_t)(value >> 8), (uint8_t)(value & 0xFF) };
    return i2c_write_timeout_us(INA219_I2C, INA219_I2C_ADDR, buf, 3, false, I2C_TIMEOUT_US) == 3;
}

static bool read_reg(uint8_t reg, uint16_t *value) {
    if (i2c_write_timeout_us(INA219_I2C, INA219_I2C_ADDR, &reg, 1, true, I2C_TIMEOUT_US) != 1)
        return false;
    uint8_t buf[2];
    if (i2c_read_timeout_us(INA219_I2C, INA219_I2C_ADDR, buf, 2, false, I2C_TIMEOUT_US) != 2)
        return false;
    *value = ((uint16_t)buf[0] << 8) | buf[1];
    return true;
}

bool ina219_init(void) {
    i2c_init(INA219_I2C, 400 * 1000);  // 400kHz
    gpio_set_function(INA219_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(INA219_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(INA219_SDA_PIN);
    gpio_pull_up(INA219_SCL_PIN);

    // Config: 32V bus range, ±320mV shunt range, 12-bit, continuous
    // Bits: BRNG=1 (32V), PGA=11 (±320mV), BADC=0011 (12-bit),
    //       SADC=0011 (12-bit), MODE=111 (continuous both)
    // Value: 0x399F
    if (!write_reg(REG_CONFIG, 0x399F)) {
        ina219_present = false;
        return false;
    }

    // Calibration register for 0.1 ohm shunt
    // Cal = trunc(0.04096 / (current_LSB * R_shunt))
    // current_LSB = max_current / 2^15 = 3.2A / 32768 ≈ 0.0001A
    // Cal = trunc(0.04096 / (0.0001 * 0.1)) = 4096
    write_reg(REG_CALIBRATE, 4096);

    ina219_present = true;
    return true;
}

void ina219_read(ina219_data_t *data) {
    data->present = ina219_present;
    if (!ina219_present) {
        data->voltage_mv = 0;
        data->current_ma = 0;
        data->power_mw = 0;
        return;
    }

    uint16_t raw;

    // Bus voltage: bits [15:3] contain voltage, LSB = 4mV
    if (read_reg(REG_BUS_V, &raw)) {
        data->voltage_mv = (raw >> 3) * 4;
    }

    // Current: signed, LSB = 0.1mA (with our calibration)
    if (read_reg(REG_CURRENT, &raw)) {
        data->current_ma = (int16_t)raw / 10;
    }

    // Power: LSB = 2mW (with our calibration)
    if (read_reg(REG_POWER, &raw)) {
        data->power_mw = raw * 2;
    }
}
