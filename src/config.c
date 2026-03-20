#include "config.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include <string.h>
#include <stdio.h>

// Store config in last sector of flash (4KB)
// RP2350 has 4MB flash, last sector at offset 0x3FF000
#define CONFIG_FLASH_OFFSET  (4 * 1024 * 1024 - FLASH_SECTOR_SIZE)

static uint32_t calc_checksum(const config_t *cfg) {
    const uint8_t *data = (const uint8_t *)cfg;
    uint32_t sum = 0;
    size_t len = offsetof(config_t, checksum);
    for (size_t i = 0; i < len; i++) {
        sum = sum * 31 + data[i];
    }
    return sum;
}

void config_defaults(config_t *cfg) {
    memset(cfg, 0, sizeof(config_t));
    cfg->magic = CONFIG_MAGIC;

    // Default calibration: full ADC range, center at midpoint
    for (int i = 0; i < 4; i++) {
        cfg->cal[i].min = 100;
        cfg->cal[i].center = 2048;
        cfg->cal[i].max = 3995;
    }

    cfg->checksum = calc_checksum(cfg);
}

bool config_load(config_t *cfg) {
    const uint8_t *flash_data = (const uint8_t *)(XIP_BASE + CONFIG_FLASH_OFFSET);
    memcpy(cfg, flash_data, sizeof(config_t));

    if (cfg->magic != CONFIG_MAGIC) {
        config_defaults(cfg);
        return false;
    }

    uint32_t expected = calc_checksum(cfg);
    if (cfg->checksum != expected) {
        config_defaults(cfg);
        return false;
    }

    return true;
}

void config_save(const config_t *cfg) {
    config_t save_cfg;
    memcpy(&save_cfg, cfg, sizeof(config_t));
    save_cfg.checksum = calc_checksum(&save_cfg);

    uint32_t irq_state = save_and_disable_interrupts();
    flash_range_erase(CONFIG_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(CONFIG_FLASH_OFFSET, (const uint8_t *)&save_cfg, sizeof(config_t));
    restore_interrupts(irq_state);
}
