#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

// Calibration data for one pot
typedef struct {
    uint16_t min;
    uint16_t center;
    uint16_t max;
    bool     reverse;     // invert direction
} cal_pot_t;

// Hardcoded calibration — measured 2026-03-19
//                              min   center  max    reverse
#define CAL_STEERING    {  234, 1990, 3629, true  }
#define CAL_THROTTLE    {  827, 2070, 3583, true  }
#define CAL_TRIM_STEER  {    5, 2045, 4085, true  }
#define CAL_TRIM_THROT  {    5, 2045, 4085, true  }

#endif
